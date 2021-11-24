/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "FSPicker"

#include <imagine/gui/FSPicker.hh>
#include <imagine/gui/TextTableView.hh>
#include <imagine/gui/TextEntry.hh>
#include <imagine/gui/NavView.hh>
#include <imagine/fs/FS.hh>
#include <imagine/base/ApplicationContext.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/logger/logger.h>
#include <imagine/util/math/int.hh>
#include <imagine/util/format.hh>
#include <string>

static bool isValidRootEndChar(char c)
{
	return c == '/' || c == '\0';
}

FSPicker::FSPicker(ViewAttachParams attach, Gfx::TextureSpan backRes, Gfx::TextureSpan closeRes,
	FilterFunc filter,  bool singleDir, Gfx::GlyphTextureSet *face_):
	View{attach},
	filter{filter},
	onClose_
	{
		[](FSPicker &picker, Input::Event e)
		{
			picker.dismiss();
		}
	},
	msgText{face_ ? face_ : &defaultFace()},
	singleDir{singleDir}
{
	const Gfx::LGradientStopDesc fsNavViewGrad[]
	{
		{ .0, Gfx::VertexColorPixelFormat.build(.5, .5, .5, 1.) },
		{ .03, Gfx::VertexColorPixelFormat.build(1. * .4, 1. * .4, 1. * .4, 1.) },
		{ .3, Gfx::VertexColorPixelFormat.build(1. * .4, 1. * .4, 1. * .4, 1.) },
		{ .97, Gfx::VertexColorPixelFormat.build(.35 * .4, .35 * .4, .35 * .4, 1.) },
		{ 1., Gfx::VertexColorPixelFormat.build(.5, .5, .5, 1.) },
	};
	auto nav = makeView<BasicNavView>
		(
			&face(),
			singleDir ? nullptr : backRes,
			closeRes
		);
	nav->setBackgroundGradient(fsNavViewGrad);
	nav->setCenterTitle(false);
	nav->setOnPushLeftBtn(
		[this](Input::Event e)
		{
			onLeftNavBtn(e);
		});
	nav->setOnPushRightBtn(
		[this](Input::Event e)
		{
			onRightNavBtn(e);
		});
	nav->setOnPushMiddleBtn(
		[this](Input::Event e)
		{
			if(!this->singleDir)
			{
				pushFileLocationsView(e);
			}
		});
	controller.setNavView(std::move(nav));
	controller.push(makeView<TableView>(text));
}

void FSPicker::place()
{
	controller.place(viewRect(), projP);
	msgText.compile(renderer(), projP);
}

void FSPicker::changeDirByInput(IG::CStringView path, FS::RootPathInfo rootInfo, bool forcePathChange, Input::Event e)
{
	auto ec = setPath(path, forcePathChange, rootInfo, e);
	if(ec && !forcePathChange)
		return;
	place();
	postDraw();
}

void FSPicker::setOnChangePath(OnChangePathDelegate del)
{
	onChangePath_ = del;
}

void FSPicker::setOnSelectFile(OnSelectFileDelegate del)
{
	onSelectFile_ = del;
}

void FSPicker::setOnClose(OnCloseDelegate del)
{
	onClose_ = del;
}

void FSPicker::onLeftNavBtn(Input::Event e)
{
	goUpDirectory(e);
}

void FSPicker::onRightNavBtn(Input::Event e)
{
	onClose_.callCopy(*this, e);
}

void FSPicker::setOnPathReadError(OnPathReadError del)
{
	onPathReadError_ = del;
}

bool FSPicker::inputEvent(Input::Event e)
{
	if(e.isDefaultCancelButton() && e.pushed())
	{
		onRightNavBtn(e);
		return true;
	}
	else if(controller.viewHasFocus() && e.pushed() && e.isDefaultLeftButton())
	{
		controller.moveFocusToNextView(e, CT2DO);
		controller.top().setFocus(false);
		return true;
	}
	else if(!isSingleDirectoryMode() && (e.pushedKey(Input::Keycode::GAME_B) || e.pushedKey(Input::Keycode::F1)))
	{
		pushFileLocationsView(e);
		return true;
	}
	return controller.inputEvent(e);
}

void FSPicker::prepareDraw()
{
	if(dir.size())
	{
		controller.top().prepareDraw();
	}
	else
	{
		msgText.makeGlyphs(renderer());
	}
	controller.navView()->prepareDraw();
}

void FSPicker::draw(Gfx::RendererCommands &cmds)
{
	if(dir.size())
	{
		controller.top().draw(cmds);
	}
	else
	{
		using namespace Gfx;
		cmds.set(ColorName::WHITE);
		cmds.setCommonProgram(CommonProgram::TEX_ALPHA, projP.makeTranslate());
		auto textRect = controller.top().viewRect();
		if(IG::isOdd(textRect.ySize()))
			textRect.y2--;
		msgText.draw(cmds, projP.unProjectRect(textRect).pos(C2DO), C2DO, projP);
	}
	controller.navView()->draw(cmds);
}

void FSPicker::onAddedToController(ViewController *, Input::Event e)
{
	controller.top().onAddedToController(&controller, e);
}

std::error_code FSPicker::setPath(IG::CStringView path, bool forcePathChange, FS::RootPathInfo rootInfo, Input::Event e)
{
	auto prevPath = currPath;
	try
	{
		auto dirIt = FS::directory_iterator{path};
		currPath = path;
		dir.clear();
		for(auto &entry : dirIt)
		{
			//logMsg("entry: %s", entry.name());
			if(filter && !filter(entry))
			{
				continue;
			}
			bool isDir = entry.type() == FS::file_type::directory;
			dir.emplace_back(entry.name().data(), isDir);
		}
		std::sort(dir.begin(), dir.end(),
			[](FileEntry e1, FileEntry e2)
			{
				if(e1.isDir && !e2.isDir)
					return true;
				else if(!e1.isDir && e2.isDir)
					return false;
				else
					return FS::fileStringNoCaseLexCompare(e1.name, e2.name);
			});
		waitForDrawFinished();
		text.clear();
		if(dir.size())
		{
			msgText.setString({});
			text.reserve(dir.size());
			for(int idx = 0; auto const &entry : dir)
			{
				if(entry.isDir)
				{
					text.emplace_back(entry.name, &face(),
						[this, idx](Input::Event e)
						{
							assert(!singleDir);
							auto filePath = pathString(dir[idx].name);
							logMsg("going to dir %s", filePath.data());
							changeDirByInput(filePath, root, false, e);
						});
				}
				else
				{
					text.emplace_back(entry.name, &face(),
						[this, idx](Input::Event e)
						{
							onSelectFile_.callCopy(*this, dir[idx].name.data(), e);
						});
				}
				idx++;
			}
		}
		else
		{
			// no entires, show a message instead
			msgText.setString("Empty Directory");
		}
	}
	catch(std::system_error &err)
	{
		logErr("can't open %s", path.data());
		auto ec = err.code();
		if(!forcePathChange)
		{
			onPathReadError_.callSafe(*this, ec);
			return ec;
		}
		msgText.setString(fmt::format("Can't open directory:\n{}\nPick a path from the top bar",
			ec.message()));
	}
	if(!e.isPointer())
		static_cast<TableView*>(&controller.top())->highlightCell(0);
	else
		static_cast<TableView*>(&controller.top())->resetScroll();
	auto pathLen = path.size();
	// verify root info
	if(rootInfo.length &&
		(rootInfo.length > pathLen || !isValidRootEndChar(path[rootInfo.length]) || rootInfo.name.empty()))
	{
		logWarn("invalid root parameters");
		rootInfo.length = 0;
	}
	if(rootInfo.length)
	{
		logMsg("root info:%d:%s", (int)rootInfo.length, rootInfo.name.data());
		root = rootInfo;
		if(pathLen > rootInfo.length)
			rootedPath = IG::format<FS::PathString>("{}{}", rootInfo.name, &path[rootInfo.length]);
		else
			rootedPath = rootInfo.name;
	}
	else
	{
		logMsg("no root info");
		root = {};
		rootedPath = currPath;
	}
	controller.top().setName(rootedPath);
	controller.navView()->showLeftBtn(!isAtRoot());
	onChangePath_.callSafe(*this, prevPath, e);
	return {};
}

std::error_code FSPicker::setPath(IG::CStringView path, bool forcePathChange, FS::RootPathInfo rootInfo)
{
	return setPath(path, forcePathChange, rootInfo, appContext().defaultInputEvent());
}

std::error_code FSPicker::setPath(FS::PathLocation location, bool forcePathChange)
{
	return setPath(location.path, forcePathChange, location.root);
}

std::error_code FSPicker::setPath(FS::PathLocation location, bool forcePathChange, Input::Event e)
{
	return setPath(location.path, forcePathChange, location.root, e);
}

FS::PathString FSPicker::path() const
{
	return currPath;
}

void FSPicker::clearSelection()
{
	controller.top().clearSelection();
}

FS::PathString FSPicker::pathString(std::string_view base) const
{
	return FS::pathString(currPath.size() > 1 ? currPath : "", base);
}

bool FSPicker::isSingleDirectoryMode() const
{
	return singleDir;
}

void FSPicker::goUpDirectory(Input::Event e)
{
	clearSelection();
	changeDirByInput(FS::dirname(currPath), root, true, e);
}

bool FSPicker::isAtRoot() const
{
	if(root.length)
	{
		auto pathLen = currPath.size();
		assumeExpr(pathLen >= root.length);
		return pathLen == root.length;
	}
	else
	{
		return currPath == "/";
	}
}

void FSPicker::pushFileLocationsView(Input::Event e)
{
	rootLocation = appContext().rootFileLocations();
	int customItems = appContext().hasSystemPathPicker() ? 3 : 2;
	auto view = makeViewWithName<TextTableView>("File Locations", rootLocation.size() + customItems);
	for(auto &loc : rootLocation)
	{
		view->appendItem(loc.description,
			[this, &loc](View &view, Input::Event e)
			{
				auto pathLen = loc.path.size();
				changeDirByInput(loc.path, loc.root, true, e);
				view.dismiss();
			});
	}
	view->appendItem("Root Filesystem",
		[this](View &view, Input::Event e)
		{
			changeDirByInput("/", {}, true, e);
			view.dismiss();
		});
	view->appendItem("Custom Path",
		[this](Input::Event e)
		{
			auto textInputView = makeView<CollectTextInputView>(
				"Input a directory path", currPath, nullptr,
				[this](CollectTextInputView &view, const char *str)
				{
					if(!str || !strlen(str))
					{
						view.dismiss();
						return false;
					}
					changeDirByInput(str, appContext().nearestRootPath(str), false, appContext().defaultInputEvent());
					dismissPrevious();
					view.dismiss();
					return false;
				});
			pushAndShow(std::move(textInputView), e);
		});
	if(appContext().hasSystemPathPicker())
	{
		view->appendItem("OS Path Picker",
			[this](View &view, Input::Event e)
			{
				appContext().showSystemPathPicker(
					[this, &view](const char *path)
					{
						changeDirByInput(path, appContext().nearestRootPath(path), false, appContext().defaultInputEvent());
						view.dismiss();
					});
			});
	}
	pushAndShow(std::move(view), e);
}

Gfx::GlyphTextureSet &FSPicker::face()
{
	return *msgText.face();
}
