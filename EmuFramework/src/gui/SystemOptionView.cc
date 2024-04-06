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

#include <emuframework/SystemOptionView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuOptions.hh>
#include <emuframework/viewUtils.hh>
#include "CPUAffinityView.hh"
#include <imagine/base/ApplicationContext.hh>
#include <imagine/gui/TextTableView.hh>
#include <imagine/fs/FS.hh>
#include <format>

namespace EmuEx
{

SystemOptionView::SystemOptionView(ViewAttachParams attach, bool customMenu):
	TableView{"系统设置", attach, item},
	autosaveTimerItem
	{
		{"关闭",    attach, {.id = 0}},
		{"5分钟",  attach, {.id = 5}},
		{"10分钟", attach, {.id = 10}},
		{"15分钟", attach, {.id = 15}},
		{"自定义数值", attach, [this](const Input::Event &e)
			{
				pushAndShowNewCollectValueRangeInputView<int, 0, maxAutosaveSaveFreq.count()>(attachParams(), e, "从0-720之间输入", "",
					[this](CollectTextInputView &, auto val)
					{
						app().autosaveManager.saveTimer.frequency = Minutes{val};
						autosaveTimer.setSelected(MenuId{val}, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	autosaveTimer
	{
		"自动存档间隔", attach,
		MenuId{app().autosaveManager.saveTimer.frequency.count()},
		autosaveTimerItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(!idx)
					return false;
				t.resetString(std::format("{}", app().autosaveManager.saveTimer.frequency));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().autosaveManager.saveTimer.frequency = IG::Minutes{item.id}; }
		},
	},
	autosaveLaunchItem
	{
		{"主插槽",            attach, {.id = AutosaveLaunchMode::Load}},
		{"主插槽 (无状态)", attach, {.id = AutosaveLaunchMode::LoadNoState}},
		{"未保存插槽",         attach, {.id = AutosaveLaunchMode::NoSave}},
		{"选择插槽",          attach, {.id = AutosaveLaunchMode::Ask}},
	},
	autosaveLaunch
	{
		"自动保存启动模式", attach,
		MenuId{app().autosaveManager.autosaveLaunchMode},
		autosaveLaunchItem,
		{
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().autosaveManager.autosaveLaunchMode = AutosaveLaunchMode(item.id.val); }
		},
	},
	autosaveContent
	{
		"自动保存内容", attach,
		app().autosaveManager.saveOnlyBackupMemory,
		"状态与备份随机存取存储器", "仅备份随机存取存储器",
		[this](BoolMenuItem &item)
		{
			app().autosaveManager.saveOnlyBackupMemory = item.flipBoolValue(*this);
		}
	},
	confirmOverwriteState
	{
		"确认覆盖状态", attach,
		app().confirmOverwriteState,
		[this](BoolMenuItem &item)
		{
			app().confirmOverwriteState = item.flipBoolValue(*this);
		}
	},
	fastModeSpeedItem
	{
		{"1.5倍",  attach, {.id = 150}},
		{"2倍",    attach, {.id = 200}},
		{"4倍",    attach, {.id = 400}},
		{"8倍",    attach, {.id = 800}},
		{"16倍",   attach, {.id = 1600}},
		{"自定义数值", attach,
			[this](const Input::Event &e)
			{
				pushAndShowNewCollectValueRangeInputView<float, 1, 20>(attachParams(), e, "输入介于1.0到20.0之间的数值", "",
					[this](CollectTextInputView &, auto val)
					{
						auto valAsInt = std::round(val * 100.f);
						app().setAltSpeed(AltSpeedMode::fast, valAsInt);
						fastModeSpeed.setSelected(MenuId{valAsInt}, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	fastModeSpeed
	{
		"快进速度", attach,
		MenuId{app().altSpeed(AltSpeedMode::fast)},
		fastModeSpeedItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{:g}x", app().altSpeedAsDouble(AltSpeedMode::fast)));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setAltSpeed(AltSpeedMode::fast, item.id); }
		},
	},
	slowModeSpeedItem
	{
		{"0.25倍", attach, {.id = 25}},
		{"0.50倍", attach, {.id = 50}},
		{"自定义数值", attach,
			[this](const Input::Event &e)
			{
				pushAndShowNewCollectValueInputView<float>(attachParams(), e, "输入0.05至1.0之间的数值", "",
					[this](CollectTextInputView &, auto val)
					{
						auto valAsInt = std::round(val * 100.f);
						if(app().setAltSpeed(AltSpeedMode::slow, valAsInt))
						{
							slowModeSpeed.setSelected(MenuId{valAsInt}, *this);
							dismissPrevious();
							return true;
						}
						else
						{
							app().postErrorMessage("值不在范围内");
							return false;
						}
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	slowModeSpeed
	{
		"慢动作速度", attach,
		MenuId{app().altSpeed(AltSpeedMode::slow)},
		slowModeSpeedItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{:g}x", app().altSpeedAsDouble(AltSpeedMode::slow)));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setAltSpeed(AltSpeedMode::slow, item.id); }
		},
	},
	rewindStatesItem
	{
		{"0",  attach, {.id = 0}},
		{"30", attach, {.id = 30}},
		{"60", attach, {.id = 60}},
		{"自定义数值", attach, [this](const Input::Event &e)
			{
				pushAndShowNewCollectValueRangeInputView<int, 0, 50000>(attachParams(), e,
					"输入0到500000之间的数值", std::to_string(app().rewindManager.maxStates),
					[this](CollectTextInputView &, auto val)
					{
						app().rewindManager.updateMaxStates(val);
						rewindStates.setSelected(val, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	rewindStates
	{
		"回溯状态", attach,
		MenuId{app().rewindManager.maxStates},
		rewindStatesItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}", app().rewindManager.maxStates));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().rewindManager.updateMaxStates(item.id); }
		},
	},
	rewindTimeInterval
	{
		"回溯状态间隔（秒）", std::to_string(app().rewindManager.saveTimer.frequency.count()), attach,
		[this](const Input::Event &e)
		{
			pushAndShowNewCollectValueRangeInputView<int, 1, 60>(attachParams(), e,
				"输入1到60之间的数值", std::to_string(app().rewindManager.saveTimer.frequency.count()),
				[this](CollectTextInputView &, auto val)
				{
					app().rewindManager.saveTimer.frequency = Seconds{val};
					rewindTimeInterval.set2ndName(std::to_string(val));
					return true;
				});
		}
	},
	performanceMode
	{
		"性能模式", attach,
		app().useSustainedPerformanceMode,
		"普通", "持续",
		[this](BoolMenuItem &item)
		{
			app().useSustainedPerformanceMode = item.flipBoolValue(*this);
		}
	},
	noopThread
	{
		"无操作线程（实验性）", attach,
		(bool)app().useNoopThread,
		[this](BoolMenuItem &item)
		{
			app().useNoopThread = item.flipBoolValue(*this);
		}
	},
	cpuAffinity
	{
		"配置CPU亲合力", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<CPUAffinityView>(appContext().cpuCount()), e);
		}
	}
{
	if(!customMenu)
	{
		loadStockItems();
	}
}

void SystemOptionView::loadStockItems()
{
	item.emplace_back(&autosaveLaunch);
	item.emplace_back(&autosaveTimer);
	item.emplace_back(&autosaveContent);
	item.emplace_back(&confirmOverwriteState);
	item.emplace_back(&fastModeSpeed);
	item.emplace_back(&slowModeSpeed);
	item.emplace_back(&rewindStates);
	item.emplace_back(&rewindTimeInterval);
	if(used(performanceMode) && appContext().hasSustainedPerformanceMode())
		item.emplace_back(&performanceMode);
	if(used(noopThread))
		item.emplace_back(&noopThread);
	if(used(cpuAffinity) && appContext().cpuCount() > 1)
		item.emplace_back(&cpuAffinity);
}

}
