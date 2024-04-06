/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/MainMenuView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuSystem.hh>
#include <emuframework/CreditsView.hh>
#include <emuframework/FilePicker.hh>
#include <emuframework/VideoOptionView.hh>
#include <emuframework/InputManagerView.hh>
#include <emuframework/TouchConfigView.hh>
#include <emuframework/BundledGamesView.hh>
#include "RecentContentView.hh"
#include "FrameTimingView.hh"
#include <emuframework/EmuOptions.hh>
#include <imagine/gui/AlertView.hh>
#include <imagine/base/ApplicationContext.hh>
#include <imagine/fs/FS.hh>
#include <imagine/bluetooth/sys.hh>
#include <imagine/bluetooth/BluetoothInputDevScanner.hh>
#include <format>
#include <imagine/logger/logger.h>

namespace EmuEx
{

constexpr SystemLogger log{"AppMenus"};

class OptionCategoryView : public TableView, public EmuAppHelper<OptionCategoryView>
{
public:
	OptionCategoryView(ViewAttachParams attach);

protected:
	TextMenuItem subConfig[8];
};

static void onScanStatus(EmuApp &app, unsigned status, int arg);

template <class ViewT>
static void handledFailedBTAdapterInit(ViewT &view, ViewAttachParams attach, const Input::Event &e)
{
	view.app().postErrorMessage("无法初始化蓝牙适配器");
	#ifdef CONFIG_BLUETOOTH_BTSTACK
	if(!FS::exists("/var/lib/dpkg/info/ch.ringwald.btstack.list"))
	{
		view.pushAndShowModal(std::make_unique<YesNoAlertView>(attach, "未找到BTstack，打开Cydia进行安装吗？",
			YesNoAlertView::Delegates
			{
				.onYes = [](View &v){ v.appContext().openURL("cydia://package/ch.ringwald.btstack"); }
			}), e, false);
	}
	#endif
}

MainMenuView::MainMenuView(ViewAttachParams attach, bool customMenu):
	TableView{EmuApp::mainViewName(), attach, item},
	loadGame
	{
		"打开游戏", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(FilePicker::forLoading(attachParams(), e), e, false);
		}
	},
	systemActions
	{
		"系统操作", attach,
		[this](const Input::Event &e)
		{
			if(!system().hasContent())
				return;
			pushAndShow(app().makeView(attachParams(), EmuApp::ViewID::SYSTEM_ACTIONS), e);
		}
	},
	recentGames
	{
		"最近游玩", attach,
		[this](const Input::Event &e)
		{
			if(app().recentContent.size())
			{
				pushAndShow(makeView<RecentContentView>(app().recentContent), e);
			}
		}
	},
	bundledGames
	{
		"捆绑内容", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<BundledGamesView>(), e);
		}
	},
	options
	{
		"设置", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<OptionCategoryView>(), e);
		}
	},
	onScreenInputManager
	{
		"虚拟按键设置", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<TouchConfigView>(app().defaultVController()), e);
		}
	},
	inputManager
	{
		"键盘/手柄输入设置", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<InputManagerView>(app().inputManager), e);
		}
	},
	benchmark
	{
		"性能测试", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(FilePicker::forBenchmarking(attachParams(), e), e, false);
		}
	},
	scanWiimotes
	{
		"扫描Wiimotes/iCP/JS1", attach,
		[this](const Input::Event &e)
		{
			if(app().bluetoothAdapter())
			{
				if(Bluetooth::scanForDevices(appContext(), *app().bluetoothAdapter(),
					[this](BluetoothAdapter &, unsigned status, int arg)
					{
						onScanStatus(app(), status, arg);
					}))
				{
					app().postMessage(4, false, "开始扫描……\n(请访问网站以获取特定设备的帮助)");
				}
				else
				{
					app().postMessage(1, false, "仍在扫描中");
				}
			}
			else
			{
				handledFailedBTAdapterInit(*this, attachParams(), e);
			}
			postDraw();
		}
	},
	bluetoothDisconnect
	{
		"蓝牙未连接", attach,
		[this](const Input::Event &e)
		{
			auto devConnected = Bluetooth::devsConnected(appContext());
			if(devConnected)
			{
				pushAndShowModal(makeView<YesNoAlertView>(std::format("Really disconnect {} Bluetooth device(s)?", devConnected),
					YesNoAlertView::Delegates{.onYes = [this]{ app().closeBluetoothConnections(); }}), e);
			}
		}
	},
	#ifdef CONFIG_BLUETOOTH_SERVER
	acceptPS3ControllerConnection
	{
		"扫描PS3控制器", attach,
		[this](const Input::Event &e)
		{
			if(app().bluetoothAdapter())
			{
				app().postMessage(4, "准备按下PS按钮");
				auto startedScan = Bluetooth::listenForDevices(appContext(), *app().bluetoothAdapter(),
					[this](BluetoothAdapter &bta, unsigned status, int arg)
					{
						switch(status)
						{
							case BluetoothAdapter::INIT_FAILED:
							{
								app().postErrorMessage(Config::envIsLinux ? 8 : 2,
									Config::envIsLinux ?
										"无法注册服务器，请确保此可执行文件已启用 cap_net_bind_service 权限，并且 bluetoothd 没有运行" :
										"蓝牙设置失败");
								break;
							}
							case BluetoothAdapter::SCAN_COMPLETE:
							{
								app().postMessage(4, "请按下您控制器上的PS按钮\n(访问网站获取配对帮助)");
								break;
							}
							default: onScanStatus(app(), status, arg);
						}
					});
				if(!startedScan)
				{
					app().postMessage(1, "仍在扫描中");
				}
			}
			else
			{
				handledFailedBTAdapterInit(*this, attachParams(), e);
			}
			postDraw();
		}
	},
	#endif
	about
	{
		"关于", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<CreditsView>(EmuSystem::creditsViewStr), e);
		}
	},
	exitApp
	{
		"退出", attach,
		[this]()
		{
			appContext().exit();
		}
	}
{
	if(!customMenu)
	{
		reloadItems();
	}
}

static void onScanStatus(EmuApp &app, unsigned status, int arg)
{
	switch(status)
	{
		case BluetoothAdapter::INIT_FAILED:
		{
			if(Config::envIsIOS)
			{
				app.postErrorMessage("BTstack 开启失败，请确保iOS的蓝牙堆栈未处于活动状态");
			}
			break;
		}
		case BluetoothAdapter::SCAN_FAILED:
		{
			app.postErrorMessage("扫描失败");
			break;
		}
		case BluetoothAdapter::SCAN_NO_DEVS:
		{
			app.postMessage("设备未找到");
			break;
		}
		case BluetoothAdapter::SCAN_PROCESSING:
		{
			app.postMessage(2, 0, std::format("Checking {} device(s)...", arg));
			break;
		}
		case BluetoothAdapter::SCAN_NAME_FAILED:
		{
			app.postErrorMessage("读取设备名称失败");
			break;
		}
		case BluetoothAdapter::SCAN_COMPLETE:
		{
			int devs = Bluetooth::pendingDevs();
			if(devs)
			{
				app.postMessage(2, 0, std::format("Connecting to {} device(s)...", devs));
				Bluetooth::connectPendingDevs(app.bluetoothAdapter());
			}
			else
			{
				app.postMessage("扫描完成，未识别到设备");
			}
			break;
		}
		/*case BluetoothAdapter::SOCKET_OPEN_FAILED:
		{
			app.postErrorMessage("Failed opening a Bluetooth connection");
		}*/
	}
};

void MainMenuView::onShow()
{
	TableView::onShow();
	log.info("刷新主菜单状态");
	recentGames.setActive(app().recentContent.size());
	systemActions.setActive(system().hasContent());
	bluetoothDisconnect.setActive(Bluetooth::devsConnected(appContext()));
}

void MainMenuView::loadFileBrowserItems()
{
	item.emplace_back(&loadGame);
	item.emplace_back(&recentGames);
	if(EmuSystem::hasBundledGames && app().showsBundledGames)
	{
		item.emplace_back(&bundledGames);
	}
}

void MainMenuView::loadStandardItems()
{
	item.emplace_back(&systemActions);
	item.emplace_back(&onScreenInputManager);
	item.emplace_back(&inputManager);
	item.emplace_back(&options);
	if(used(scanWiimotes) && app().showsBluetoothScan)
	{
		item.emplace_back(&scanWiimotes);
		#ifdef CONFIG_BLUETOOTH_SERVER
		item.emplace_back(&acceptPS3ControllerConnection);
		#endif
		item.emplace_back(&bluetoothDisconnect);
	}
	item.emplace_back(&benchmark);
	item.emplace_back(&about);
	item.emplace_back(&exitApp);
}

void MainMenuView::reloadItems()
{
	item.clear();
	loadFileBrowserItems();
	loadStandardItems();
}

OptionCategoryView::OptionCategoryView(ViewAttachParams attach):
	TableView
	{
		"设置",
		attach,
		[this](const TableView &) { return EmuApp::hasGooglePlayStoreFeatures() ? std::size(subConfig) : std::size(subConfig)-1; },
		[this](const TableView &, size_t idx) -> MenuItem& { return subConfig[idx]; }
	},
	subConfig
	{
		{
			"帧管理", attach,
			[this](const Input::Event &e)
			{
				pushAndShow(makeView<FrameTimingView>(), e);
			}
		},
		{
			"视频", attach,
			[this](const Input::Event &e)
			{
				pushAndShow(app().makeView(attachParams(), EmuApp::ViewID::VIDEO_OPTIONS), e);
			}
		},
		{
			"音频", attach,
			[this](const Input::Event &e)
			{
				pushAndShow(app().makeView(attachParams(), EmuApp::ViewID::AUDIO_OPTIONS), e);
			}
		},
		{
			"系统", attach,
			[this](const Input::Event &e)
			{
				pushAndShow(app().makeView(attachParams(), EmuApp::ViewID::SYSTEM_OPTIONS), e);
			}
		},
		{
			"文件路径", attach,
			[this](const Input::Event &e)
			{
				pushAndShow(app().makeView(attachParams(), EmuApp::ViewID::FILE_PATH_OPTIONS), e);
			}
		},
		{
			"图形", attach,
			[this](const Input::Event &e)
			{
				pushAndShow(app().makeView(attachParams(), EmuApp::ViewID::GUI_OPTIONS), e);
			}
		},
		{
			"在线文档", attach,
			[this]
			{
				appContext().openURL("https://www.explusalpha.com/contents/emuex/documentation");
			}
		}
	}
{
	if(EmuApp::hasGooglePlayStoreFeatures())
	{
		subConfig[lastIndex(subConfig)] =
		{
			"Beta测试参与/退出", attach,
			[this]()
			{
				appContext().openURL(std::format("https://play.google.com/apps/testing/{}", appContext().applicationId));
			}
		};
	}
}

}
