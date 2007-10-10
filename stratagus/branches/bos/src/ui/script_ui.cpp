//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name script_ui.cpp - The ui ccl functions. */
//
//      (c) Copyright 1999-2006 by Lutz Sammer, Jimmy Salmon, Martin Renold
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//
//      $Id$

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stratagus.h"
#include "script.h"
#include "interface.h"
#include "ui.h"
#include "video.h"
#include "map.h"
#include "menus.h"
#include "font.h"
#include "util.h"
#include "unit.h"
#include "unittype.h"
#include "spells.h"

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

char *ClickMissile;              /// FIXME:docu
char *DamageMissile;             /// FIXME:docu

typedef struct _info_text_ {
	char *Text;                  /// FIXME:docu
	CFont *Font;                 /// FIXME:docu
	int X;                       /// FIXME:docu
	int Y;                       /// FIXME:docu
} InfoText;                      /// FIXME:docu

std::map<std::string, ButtonStyle *> ButtonStyleHash;
std::map<std::string, CheckboxStyle *> CheckboxStyleHash;

std::vector<CUnitInfoPanel *> AllPanels; /// Array of panels.

static int HandleCount = 1;     /// Lua handler count

CPreference Preference;

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

/**
**  Set speed of middle-mouse scroll
**
**  @param l  Lua state.
*/
static int CclSetMouseScrollSpeedDefault(lua_State *l)
{
	LuaCheckArgs(l, 1);
	UI.MouseScrollSpeedDefault = LuaToNumber(l, 1);
	return 0;
}

/**
**  Set speed of ctrl-middle-mouse scroll
**
**  @param l  Lua state.
*/
static int CclSetMouseScrollSpeedControl(lua_State *l)
{
	LuaCheckArgs(l, 1);
	UI.MouseScrollSpeedControl = LuaToNumber(l, 1);
	return 0;
}

/**
**  Set which missile is used for right click
**
**  @param l  Lua state.
*/
static int CclSetClickMissile(lua_State *l)
{
	int args;

	args = lua_gettop(l);
	if (args > 1 || (args == 1 && (!lua_isnil(l, 1) && !lua_isstring(l, 1)))) {
		LuaError(l, "incorrect argument");
	}
	delete[] ClickMissile;
	ClickMissile = NULL;
	if (args == 1 && !lua_isnil(l, 1)) {
		ClickMissile = new_strdup(lua_tostring(l, 1));
	}

	return 0;
}

/**
**  Set which missile shows Damage
**
**  @param l  Lua state.
*/
static int CclSetDamageMissile(lua_State *l)
{
	int args;

	args = lua_gettop(l);
	if (args > 1 || (args == 1 && (!lua_isnil(l, 1) && !lua_isstring(l, 1)))) {
		LuaError(l, "incorrect argument");
	}
	delete[] DamageMissile;
	DamageMissile = NULL;
	if (args == 1 && !lua_isnil(l, 1)) {
		DamageMissile = new_strdup(lua_tostring(l, 1));
	}

	return 0;
}

/**
**  Set the video resolution.
**
**  @param l  Lua state.
*/
static int CclSetVideoResolution(lua_State *l)
{
	LuaCheckArgs(l, 2);
	if (CclInConfigFile) {
		// May have been set from the command line
		if (!Video.Width || !Video.Height) {
			Video.Width = LuaToNumber(l, 1);
			Video.Height = LuaToNumber(l, 2);
		}
	}
	return 0;
}

/**
**  Get the video resolution.
**
**  @param l  Lua state.
*/
static int CclGetVideoResolution(lua_State *l)
{
	LuaCheckArgs(l, 0);
	lua_pushnumber(l, Video.Width);
	lua_pushnumber(l, Video.Height);
	return 2;
}

/**
**  Set the video fullscreen mode.
**
**  @param l  Lua state.
*/
static int CclSetVideoFullScreen(lua_State *l)
{
	LuaCheckArgs(l, 1);
	if (CclInConfigFile) {
		// May have been set from the command line
		if (!VideoForceFullScreen) {
			Video.FullScreen = LuaToBoolean(l, 1);
		}
	}
	return 0;
}

/**
**  Get the video fullscreen mode.
**
**  @param l  Lua state.
*/
static int CclGetVideoFullScreen(lua_State *l)
{
	LuaCheckArgs(l, 0);
	lua_pushboolean(l, Video.FullScreen);
	return 1;
}

/**
**  Default title screens.
**
**  @param l  Lua state.
*/
static int CclSetTitleScreens(lua_State *l)
{
	const char *value;
	int i;
	int args;
	int j;
	int subargs;
	int k;

	if (TitleScreens) {
		for (i = 0; TitleScreens[i]; ++i) {
			delete[] TitleScreens[i]->File;
			delete[] TitleScreens[i]->Music;
			if (TitleScreens[i]->Labels) {
				for (j = 0; TitleScreens[i]->Labels[j]; ++j) {
					delete[] TitleScreens[i]->Labels[j]->Text;
					delete TitleScreens[i]->Labels[j];
				}
				delete[] TitleScreens[i]->Labels;
			}
			delete TitleScreens[i];
		}
		delete[] TitleScreens;
		TitleScreens = NULL;
	}

	args = lua_gettop(l);
	TitleScreens = new TitleScreen *[args + 1];
	memset(TitleScreens, 0, (args + 1) * sizeof(TitleScreen *));

	for (j = 0; j < args; ++j) {
		if (!lua_istable(l, j + 1)) {
			LuaError(l, "incorrect argument");
		}
		TitleScreens[j] = new TitleScreen;
		TitleScreens[j]->Iterations = 1;
		lua_pushnil(l);
		while (lua_next(l, j + 1)) {
			value = LuaToString(l, -2);
			if (!strcmp(value, "Image")) {
				TitleScreens[j]->File = new_strdup(LuaToString(l, -1));
			} else if (!strcmp(value, "Music")) {
				TitleScreens[j]->Music = new_strdup(LuaToString(l, -1));
			} else if (!strcmp(value, "Timeout")) {
				TitleScreens[j]->Timeout = LuaToNumber(l, -1);
			} else if (!strcmp(value, "Iterations")) {
				TitleScreens[j]->Iterations = LuaToNumber(l, -1);
			} else if (!strcmp(value, "Labels")) {
				if (!lua_istable(l, -1)) {
					LuaError(l, "incorrect argument");
				}
				subargs = luaL_getn(l, -1);
				TitleScreens[j]->Labels = new TitleScreenLabel *[subargs + 1];
				memset(TitleScreens[j]->Labels, 0, (subargs + 1) * sizeof(TitleScreenLabel *));
				for (k = 0; k < subargs; ++k) {
					lua_rawgeti(l, -1, k + 1);
					if (!lua_istable(l, -1)) {
						LuaError(l, "incorrect argument");
					}
					TitleScreens[j]->Labels[k] = new TitleScreenLabel;
					lua_pushnil(l);
					while (lua_next(l, -2)) {
						value = LuaToString(l, -2);
						if (!strcmp(value, "Text")) {
							TitleScreens[j]->Labels[k]->Text = new_strdup(LuaToString(l, -1));
						} else if (!strcmp(value, "Font")) {
							TitleScreens[j]->Labels[k]->Font = CFont::Get(LuaToString(l, -1));
						} else if (!strcmp(value, "Pos")) {
							if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
								LuaError(l, "incorrect argument");
							}
							lua_rawgeti(l, -1, 1);
							TitleScreens[j]->Labels[k]->Xofs = LuaToNumber(l, -1);
							lua_pop(l, 1);
							lua_rawgeti(l, -1, 2);
							TitleScreens[j]->Labels[k]->Yofs = LuaToNumber(l, -1);
							lua_pop(l, 1);
						} else if (!strcmp(value, "Flags")) {
							int subsubargs;
							int subk;

							if (!lua_istable(l, -1)) {
								LuaError(l, "incorrect argument");
							}
							subsubargs = luaL_getn(l, -1);
							for (subk = 0; subk < subsubargs; ++subk) {
								lua_rawgeti(l, -1, subk + 1);
								value = LuaToString(l, -1);
								lua_pop(l, 1);
								if (!strcmp(value, "center")) {
									TitleScreens[j]->Labels[k]->Flags |= TitleFlagCenter;
								} else {
									LuaError(l, "incorrect flag");
								}
							}
						} else {
							LuaError(l, "Unsupported key: %s" _C_ value);
						}
						lua_pop(l, 1);
					}
					lua_pop(l, 1);
				}
			} else {
				LuaError(l, "Unsupported key: %s" _C_ value);
			}
			lua_pop(l, 1);
		}
	}

	return 0;
}

/**
**  Default menu music.
**
**  @param l  Lua state.
*/
static int CclSetMenuMusic(lua_State *l)
{
	LuaCheckArgs(l, 1);
	delete[] MenuMusic;
	MenuMusic = new_strdup(LuaToString(l, 1));
	return 0;
}

/**
**  Process a menu.
**
**  @param l  Lua state.
*/
static int CclProcessMenu(lua_State *l)
{
	int args;
	int loop;
	const char *mid;

	args = lua_gettop(l);
	if (args != 1 && args != 2) {
		LuaError(l, "incorrect argument");
	}
	mid = LuaToString(l, 1);
	if (args == 2) {
		loop = LuaToNumber(l, 2);
	} else {
		loop = 0;
	}

	if (!FindMenu(mid)) {
		LuaError(l, "menu not found: %s" _C_ mid);
	} else {
		ProcessMenu(mid, loop);
	}

	return 0;
}

/**
**  Define a cursor.
**
**  @param l  Lua state.
*/
static int CclDefineCursor(lua_State *l)
{
	const char *value;
	const char *name;
	const char *race;
	const char *file;
	int hotx;
	int hoty;
	int w;
	int h;
	int rate;
	int i;
	CCursor *ct;

	LuaCheckArgs(l, 1);
	if (!lua_istable(l, 1)) {
		LuaError(l, "incorrect argument");
	}
	name = race = file = NULL;
	hotx = hoty = w = h = rate = 0;
	lua_pushnil(l);
	while (lua_next(l, 1)) {
		value = LuaToString(l, -2);
		if (!strcmp(value, "Name")) {
			name = LuaToString(l, -1);
		} else if (!strcmp(value, "Race")) {
			race = LuaToString(l, -1);
		} else if (!strcmp(value, "File")) {
			file = LuaToString(l, -1);
		} else if (!strcmp(value, "HotSpot")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			hotx = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			hoty = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "Size")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			w = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			h = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "Rate")) {
			rate = LuaToNumber(l, -1);
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
		lua_pop(l, 1);
	}

	Assert(name && file && w && h);

	if (!strcmp(race, "any")) {
		race = NULL;
	}

	//
	//  Look if this kind of cursor already exists.
	//
	ct = NULL;
	i = 0;
	if (AllCursors.size()) {
		for (; i < (int)AllCursors.size(); ++i) {
			//
			//  Race not same, not found.
			//
			if (AllCursors[i].Race && race) {
				if (strcmp(AllCursors[i].Race, race)) {
					continue;
				}
			} else if (AllCursors[i].Race != race) {
				continue;
			}
			if (!strcmp(AllCursors[i].Ident, name)) {
				ct = &AllCursors[i];
				break;
			}
		}
	}
	//
	//  Not found, make a new slot.
	//
	if (!ct) {
		CCursor c;
		AllCursors.push_back(c);
		ct = &AllCursors.back();
		ct->Ident = new_strdup(name);
		ct->Race = race ? new_strdup(race) : NULL;
	}

	ct->G = CGraphic::New(file, w, h);
	ct->HotX = hotx;
	ct->HotY = hoty;
	ct->FrameRate = rate;

	return 0;
}

/**
**  Set the current game cursor.
**
**  @param l  Lua state.
*/
static int CclSetGameCursor(lua_State *l)
{
	LuaCheckArgs(l, 1);
	GameCursor = CursorByIdent(LuaToString(l, 1));
	return 0;
}

/**
**  Define a menu item
**
**  @param l      Lua state.
**  @param value  Button type.
*/
static MenuButtonId scm2buttonid(lua_State *l, const char *value)
{
	MenuButtonId id;

	if (!strcmp(value, "up-arrow")) {
		id = MBUTTON_UP_ARROW;
	} else if (!strcmp(value, "down-arrow")) {
		id = MBUTTON_DOWN_ARROW;
	} else if (!strcmp(value, "left-arrow")) {
		id = MBUTTON_LEFT_ARROW;
	} else if (!strcmp(value, "right-arrow")) {
		id = MBUTTON_RIGHT_ARROW;
	} else if (!strcmp(value, "s-knob")) {
		id = MBUTTON_S_KNOB;
	} else if (!strcmp(value, "s-vcont")) {
		id = MBUTTON_S_VCONT;
	} else if (!strcmp(value, "s-hcont")) {
		id = MBUTTON_S_HCONT;
	} else if (!strcmp(value, "pulldown")) {
		id = MBUTTON_PULLDOWN;
	} else if (!strcmp(value, "vthin")) {
		id = MBUTTON_VTHIN;
	} else {
		LuaError(l, "Unsupported button: %s" _C_ value);
		id = 0;
	}
	return id;
}

/**
**  Return enum from string about variable component.
**
**  @param l Lua State.
**  @param s string to convert.
**
**  @return  Corresponding value.
**  @note    Stop on error.
*/
EnumVariable Str2EnumVariable(lua_State *l, const char *s)
{
	static struct {
		const char *s;
		EnumVariable e;
	} list[] = {
		{"Value", VariableValue},
		{"Max", VariableMax},
		{"Increase", VariableIncrease},
		{"Diff", VariableDiff},
		{"Percent", VariablePercent},
		{"Name", VariableName},
		{0, VariableValue}}; // List of possible values.
	int i; // Iterator.

	for (i = 0; list[i].s; i++) {
		if (!strcmp(s, list[i].s)) {
			return list[i].e;
		}
	}
	LuaError(l, "'%s' is a invalid variable component" _C_ s);
	return VariableValue;
}

/**
**  Return enum from string about variable component.
**
**  @param l Lua State.
**  @param s string to convert.
**
**  @return  Corresponding value.
**  @note    Stop on error.
*/
static EnumUnit Str2EnumUnit(lua_State *l, const char *s)
{
	static struct {
		const char *s;
		EnumUnit e;
	} list[] = {
		{"ItSelf", UnitRefItSelf},
		{"Inside", UnitRefInside},
		{"Container", UnitRefContainer},
		{"Worker", UnitRefWorker},
		{"Goal", UnitRefGoal},
		{0, UnitRefItSelf}}; // List of possible values.
	int i; // Iterator.

	for (i = 0; list[i].s; i++) {
		if (!strcmp(s, list[i].s)) {
			return list[i].e;
		}
	}
	LuaError(l, "'%s' is a invalid Unit reference" _C_ s);
	return UnitRefItSelf;
}

/**
**  Parse the condition Panel.
**
**  @param l   Lua State.
*/
static ConditionPanel *ParseConditionPanel(lua_State *l)
{
	ConditionPanel *condition; // Condition parsed
	const char *key;           // key of lua table.
	int i;                     // iterator for flags and variable.

	Assert(lua_istable(l, -1));

	condition = new ConditionPanel;
	for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
		key = LuaToString(l, -2);
		if (!strcmp(key, "ShowOnlySelected")) {
			condition->ShowOnlySelected = LuaToBoolean(l, -1);
			} else if (!strcmp(key, "HideNeutral")) {
				condition->HideNeutral = LuaToBoolean(l, -1);
			} else if (!strcmp(key, "HideAllied")) {
				condition->HideAllied = LuaToBoolean(l, -1);
			} else if (!strcmp(key, "ShowOpponent")) {
				condition->ShowOpponent = LuaToBoolean(l, -1);
		} else {
			for (i = 0; i < UnitTypeVar.NumberBoolFlag; ++i) {
				if (!strcmp(key, UnitTypeVar.BoolFlagName[i])) {
					if (!condition->BoolFlags) {
						condition->BoolFlags = new char[UnitTypeVar.NumberBoolFlag];
						memset(condition->BoolFlags, 0, UnitTypeVar.NumberBoolFlag * sizeof(char));
					}
					condition->BoolFlags[i] = Ccl2Condition(l, LuaToString(l, -1));
					break;
				}
			}
			if (i != UnitTypeVar.NumberBoolFlag) { // key is a flag
				continue;
			}
			i = GetVariableIndex(key);
			if (i != -1) {
				if (!condition->Variables) {
					condition->Variables = new char[UnitTypeVar.NumberVariable];
					memset(condition->Variables, 0, UnitTypeVar.NumberVariable * sizeof(char));
				}
				condition->Variables[i] = Ccl2Condition(l, LuaToString(l, -1));
				continue;
			}
			LuaError(l, "'%s' invalid for Condition in DefinePanels" _C_ key);
		}
	}
	return condition;
}

static CContentType *CclParseContent(lua_State *l)
{
	CContentType *content;
	const char *key;
	int posX;
	int posY;
	ConditionPanel *condition;

	Assert(lua_istable(l, -1));
	content = NULL;
	condition = NULL;
	for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
		key = LuaToString(l, -2);
		if (!strcmp(key, "Pos")) {
			Assert(lua_istable(l, -1));
			lua_rawgeti(l, -1, 1); // X
			lua_rawgeti(l, -2, 2); // Y
			posX = LuaToNumber(l, -2);
			posY = LuaToNumber(l, -1);
			lua_pop(l, 2); // Pop X and Y
		} else if (!strcmp(key, "More")) {
			Assert(lua_istable(l, -1));
			lua_rawgeti(l, -1, 1); // Method name
			lua_rawgeti(l, -2, 2); // Method data
			key = LuaToString(l, -2);
			if (!strcmp(key, "Text")) {
				CContentTypeText *contenttext = new CContentTypeText;

				Assert(lua_istable(l, -1) || lua_isstring(l, -1));
				if (lua_isstring(l, -1)) {
					contenttext->Text = CclParseStringDesc(l);
					lua_pushnil(l); // ParseStringDesc eat token
				} else {
					for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
						key = LuaToString(l, -2);
						if (!strcmp(key, "Text")) {
							contenttext->Text = CclParseStringDesc(l);
							lua_pushnil(l); // ParseStringDesc eat token
						} else if (!strcmp(key, "Font")) {
							contenttext->Font = CFont::Get(LuaToString(l, -1));
						} else if (!strcmp(key, "Centered")) {
							contenttext->Centered = LuaToBoolean(l, -1);
						} else if (!strcmp(key, "Variable")) {
							contenttext->Index = GetVariableIndex(LuaToString(l, -1));
							if (contenttext->Index == -1) {
								LuaError(l, "unknown variable '%s'" _C_ LuaToString(l, -1));
							}
						} else if (!strcmp(key, "Component")) {
							contenttext->Component = Str2EnumVariable(l, LuaToString(l, -1));
						} else if (!strcmp(key, "Stat")) {
							contenttext->Stat = LuaToBoolean(l, -1);
						} else if (!strcmp(key, "ShowName")) {
							contenttext->ShowName = LuaToBoolean(l, -1);
						} else {
							LuaError(l, "'%s' invalid for method 'Text' in DefinePanels" _C_ key);
						}
					}
				}
				content = contenttext;
			} else if (!strcmp(key, "FormattedText")) {
				CContentTypeFormattedText *contentformattedtext = new CContentTypeFormattedText;

				Assert(lua_istable(l, -1));
				for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
					key = LuaToString(l, -2);
					if (!strcmp(key, "Format")) {
						contentformattedtext->Format = new_strdup(LuaToString(l, -1));
					} else if (!strcmp(key, "Font")) {
						contentformattedtext->Font = CFont::Get(LuaToString(l, -1));
					} else if (!strcmp(key, "Variable")) {
						contentformattedtext->Index = GetVariableIndex(LuaToString(l, -1));
						if (contentformattedtext->Index == -1) {
							LuaError(l, "unknown variable '%s'" _C_ LuaToString(l, -1));
						}
					} else if (!strcmp(key, "Component")) {
						contentformattedtext->Component = Str2EnumVariable(l, LuaToString(l, -1));
					} else if (!strcmp(key, "Centered")) {
						contentformattedtext->Centered = LuaToBoolean(l, -1);
					} else {
						LuaError(l, "'%s' invalid for method 'FormattedText' in DefinePanels" _C_ key);
					}
				}
				content = contentformattedtext;
			} else if (!strcmp(key, "FormattedText2")) {
				CContentTypeFormattedText2 *contentformattedtext2 = new CContentTypeFormattedText2;

				Assert(lua_istable(l, -1));
				for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
					key = LuaToString(l, -2);
						if (!strcmp(key, "Format")) {
							contentformattedtext2->Format = new_strdup(LuaToString(l, -1));
						} else if (!strcmp(key, "Font")) {
							contentformattedtext2->Font = CFont::Get(LuaToString(l, -1));
						} else if (!strcmp(key, "Variable")) {
						contentformattedtext2->Index1 = GetVariableIndex(LuaToString(l, -1));
						contentformattedtext2->Index2 = GetVariableIndex(LuaToString(l, -1));
						if (contentformattedtext2->Index1 == -1) {
							LuaError(l, "unknown variable '%s'" _C_ LuaToString(l, -1));
						}
					} else if (!strcmp(key, "Component")) {
						contentformattedtext2->Component1 = Str2EnumVariable(l, LuaToString(l, -1));
						contentformattedtext2->Component2 = Str2EnumVariable(l, LuaToString(l, -1));
					} else if (!strcmp(key, "Variable1")) {
						contentformattedtext2->Index1 = GetVariableIndex(LuaToString(l, -1));
						if (contentformattedtext2->Index1 == -1) {
							LuaError(l, "unknown variable '%s'" _C_ LuaToString(l, -1));
						}
					} else if (!strcmp(key, "Component1")) {
						contentformattedtext2->Component1 = Str2EnumVariable(l, LuaToString(l, -1));
					} else if (!strcmp(key, "Variable2")) {
						contentformattedtext2->Index2 = GetVariableIndex(LuaToString(l, -1));
						if (contentformattedtext2->Index2 == -1) {
							LuaError(l, "unknown variable '%s'" _C_ LuaToString(l, -1));
						}
					} else if (!strcmp(key, "Component2")) {
						contentformattedtext2->Component2 = Str2EnumVariable(l, LuaToString(l, -1));
					} else if (!strcmp(key, "Centered")) {
						contentformattedtext2->Centered = LuaToBoolean(l, -1);
					} else {
						LuaError(l, "'%s' invalid for method 'FormattedText2' in DefinePanels" _C_ key);
					}
				}
				content = contentformattedtext2;
			} else if (!strcmp(key, "Icon")) {
				CContentTypeIcon *contenticon = new CContentTypeIcon;

				for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
					key = LuaToString(l, -2);
					if (!strcmp(key, "Unit")) {
						contenticon->UnitRef = Str2EnumUnit(l, LuaToString(l, -1));
					} else {
						LuaError(l, "'%s' invalid for method 'Icon' in DefinePanels" _C_ key);
					}
				}
				content = contenticon;
			} else if (!strcmp(key, "LifeBar")) {
				CContentTypeLifeBar *contentlifebar = new CContentTypeLifeBar;

				for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
					key = LuaToString(l, -2);
					if (!strcmp(key, "Variable")) {
						contentlifebar->Index = GetVariableIndex(LuaToString(l, -1));
						if (contentlifebar->Index == -1) {
							LuaError(l, "unknown variable '%s'" _C_ LuaToString(l, -1));
						}
					} else if (!strcmp(key, "Height")) {
						contentlifebar->Height = LuaToNumber(l, -1);
					} else if (!strcmp(key, "Width")) {
						contentlifebar->Width = LuaToNumber(l, -1);
					} else {
						LuaError(l, "'%s' invalid for method 'LifeBar' in DefinePanels" _C_ key);
					}
				}
				// Default value and checking errors.
				if (contentlifebar->Height <= 0) {
					contentlifebar->Height = 5; // Default value.
				}
				if (contentlifebar->Width <= 0) {
					contentlifebar->Width = 50; // Default value.
				}
				if (contentlifebar->Index == -1) {
					LuaError(l, "variable undefined for LifeBar");
				}
				content = contentlifebar;
			} else if (!strcmp(key, "CompleteBar")) {
				CContentTypeCompleteBar *contenttypecompletebar = new CContentTypeCompleteBar;

				for (lua_pushnil(l); lua_next(l, -2); lua_pop(l, 1)) {
					key = LuaToString(l, -2);
					if (!strcmp(key, "Variable")) {
						contenttypecompletebar->Index = GetVariableIndex(LuaToString(l, -1));
						if (contenttypecompletebar->Index == -1) {
							LuaError(l, "unknown variable '%s'" _C_ LuaToString(l, -1));
						}
					} else if (!strcmp(key, "Height")) {
						contenttypecompletebar->Height = LuaToNumber(l, -1);
					} else if (!strcmp(key, "Width")) {
						contenttypecompletebar->Width = LuaToNumber(l, -1);
					} else if (!strcmp(key, "Border")) {
						contenttypecompletebar->Border = LuaToBoolean(l, -1);
					} else {
						LuaError(l, "'%s' invalid for method 'CompleteBar' in DefinePanels" _C_ key);
					}
				}
				// Default value and checking errors.
				if (contenttypecompletebar->Height <= 0) {
					contenttypecompletebar->Height = 5; // Default value.
				}
				if (contenttypecompletebar->Width <= 0) {
					contenttypecompletebar->Width = 50; // Default value.
				}
				if (contenttypecompletebar->Index == -1) {
					LuaError(l, "variable undefined for CompleteBar");
				}
				content = contenttypecompletebar;
			} else {
				LuaError(l, "Invalid drawing method '%s' in DefinePanels" _C_ key);
			}
			lua_pop(l, 2); // Pop Variable Name and Method
		} else if (!strcmp(key, "Condition")) {
			condition = ParseConditionPanel(l);
		} else {
			LuaError(l, "'%s' invalid for Contents in DefinePanels" _C_ key);
		}
	}
	content->PosX = posX;
	content->PosY = posY;
	content->Condition = condition;
	return content;
}


/**
**  Define the Panels.
**  Define what is shown in the panel(text, icon, variables)
**
**  @param l  Lua state.
**  @return   0.
*/
static int CclDefinePanelContents(lua_State *l)
{
	int i;                  // iterator for arguments.
	int j;                  // iterator for contents and panels.
	int nargs;              // number of arguments.
	const char *key;        // key of lua table.
	CUnitInfoPanel *infopanel;   // variable for transit.

	nargs = lua_gettop(l);
	for (i = 0; i < nargs; i++) {
		Assert(lua_istable(l, i + 1));
		infopanel = new CUnitInfoPanel;
		for (lua_pushnil(l); lua_next(l, i + 1); lua_pop(l, 1)) {
			key = LuaToString(l, -2);
			if (!strcmp(key, "Ident")) {
				infopanel->Name = new_strdup(LuaToString(l, -1));
			} else if (!strcmp(key, "Pos")) {
				Assert(lua_istable(l, -1));
				lua_rawgeti(l, -1, 1); // X
				lua_rawgeti(l, -2, 2); // Y
				infopanel->PosX = LuaToNumber(l, -2);
				infopanel->PosY = LuaToNumber(l, -1);
				lua_pop(l, 2); // Pop X and Y
			} else if (!strcmp(key, "DefaultFont")) {
				infopanel->DefaultFont = CFont::Get(LuaToString(l, -1));
			} else if (!strcmp(key, "Condition")) {
				infopanel->Condition = ParseConditionPanel(l);
			} else if (!strcmp(key, "Contents")) {
				Assert(lua_istable(l, -1));
				for (j = 0; j < luaL_getn(l, -1); j++, lua_pop(l, 1)) {
					lua_rawgeti(l, -1, j + 1);
					infopanel->Contents.push_back(CclParseContent(l));
				}
			} else {
				LuaError(l, "'%s' invalid for DefinePanels" _C_ key);
			}
		}
		for (std::vector<CContentType *>::iterator content = infopanel->Contents.begin();
			content != infopanel->Contents.end(); ++content) { // Default value for invalid value.
			(*content)->PosX += infopanel->PosX;
			(*content)->PosY += infopanel->PosY;
		}
		for (j = 0; j < (int)AllPanels.size(); ++j) {
			if (!strcmp(infopanel->Name, AllPanels[j]->Name)) {
				DebugPrint("Redefinition of Panel '%s'" _C_ infopanel->Name);
				delete AllPanels[j];
				AllPanels[j] = infopanel;
				break;
			}
		}
		if (j == (int)AllPanels.size()) {
			AllPanels.push_back(infopanel);
		}
	}
	return 0;
}

/**
**  Define the viewports.
**
**  @param l  Lua state.
*/
static int CclDefineViewports(lua_State *l)
{
	const char *value;
	CUserInterface *ui;
	int i;
	int args;
	int j;
	int slot;

	i = 0;
	ui = &UI;
	args = lua_gettop(l);
	for (j = 0; j < args; ++j) {
		value = LuaToString(l, j + 1);
		++j;
		if (!strcmp(value, "mode")) {
			ui->ViewportMode = (ViewportModeType)(int)LuaToNumber(l, j + 1);
		} else if (!strcmp(value, "viewport")) {
			if (!lua_istable(l, j + 1) && luaL_getn(l, j + 1) != 3) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, j + 1, 1);
			ui->Viewports[i].MapX = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, j + 1, 2);
			ui->Viewports[i].MapY = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, j + 1, 3);
			slot = (int)LuaToNumber(l, -1);
			if (slot != -1) {
				ui->Viewports[i].Unit = UnitSlots[slot];
			}
			lua_pop(l, 1);
			++i;
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
	}
	ui->NumViewports = i;

	return 0;
}

/**
**  Enable/disable display of command keys in panels.
**
**  @param l  Lua state.
*/
static int CclSetShowCommandKey(lua_State *l)
{
	LuaCheckArgs(l, 1);
	UI.ButtonPanel.ShowCommandKey = LuaToBoolean(l, 1);
	UI.ButtonPanel.Update();
	return 0;
}

/**
**  Fighter right button attacks as default.
**
**  @param l  Lua state.
*/
static int CclRightButtonAttacks(lua_State *l)
{
	LuaCheckArgs(l, 0);
	RightButtonAttacks = 1;
	return 0;
}

/**
**  Fighter right button moves as default.
**
**  @param l  Lua state.
*/
static int CclRightButtonMoves(lua_State *l)
{
	LuaCheckArgs(l, 0);
	RightButtonAttacks = 0;
	return 0;
}

/**
**  Find a button style
**
**  @param style  Name of the style to find.
**
**  @return       Button style, NULL if not found.
*/
ButtonStyle *FindButtonStyle(const char *style)
{
	return ButtonStyleHash[style];
}

/**
**  Find a checkbox style
**
**  @param style  Name of the style to find.
**
**  @return       Checkbox style, NULL if not found.
*/
CheckboxStyle *FindCheckboxStyle(const char *style)
{
	return CheckboxStyleHash[style];
}

/**
**  Parse button style properties
**
**  @param l  Lua state.
**  @param p  Properties to fill in.
*/
static void ParseButtonStyleProperties(lua_State *l, ButtonStyleProperties *p)
{
	const char *value;
	char *file;
	int w;
	int h;

	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument");
	}

	file = NULL;
	w = h = 0;

	lua_pushnil(l);
	while (lua_next(l, -2)) {
		value = LuaToString(l, -2);
		if (!strcmp(value, "File")) {
			file = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "Size")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			w = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			h = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "Frame")) {
			p->Frame = LuaToNumber(l, -1);
		} else if (!strcmp(value, "Border")) {
			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			lua_pushnil(l);
			while (lua_next(l, -2)) {
				value = LuaToString(l, -2);
				if (!strcmp(value, "Color")) {
					if (!lua_istable(l, -1) || luaL_getn(l, -1) != 3) {
						LuaError(l, "incorrect argument");
					}
					lua_rawgeti(l, -1, 1);
					p->BorderColorRGB.r = LuaToNumber(l, -1);
					lua_pop(l, 1);
					lua_rawgeti(l, -1, 2);
					p->BorderColorRGB.g = LuaToNumber(l, -1);
					lua_pop(l, 1);
					lua_rawgeti(l, -1, 3);
					p->BorderColorRGB.b = LuaToNumber(l, -1);
					lua_pop(l, 1);
				} else if (!strcmp(value, "Size")) {
					p->BorderSize = LuaToNumber(l, -1);
				} else {
					LuaError(l, "Unsupported tag: %s" _C_ value);
				}
				lua_pop(l, 1);
			}
		} else if (!strcmp(value, "TextPos")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			p->TextX = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			p->TextY = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "TextAlign")) {
			value = LuaToString(l, -1);
			if (!strcmp(value, "Center")) {
				p->TextAlign = TextAlignCenter;
			} else if (!strcmp(value, "Right")) {
				p->TextAlign = TextAlignRight;
			} else if (!strcmp(value, "Left")) {
				p->TextAlign = TextAlignLeft;
			} else {
				LuaError(l, "Invalid text alignment: %s" _C_ value);
			}
		} else if (!strcmp(value, "TextNormalColor")) {
			delete[] p->TextNormalColor;
			p->TextNormalColor = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "TextReverseColor")) {
			delete[] p->TextReverseColor;
			p->TextReverseColor = new_strdup(LuaToString(l, -1));
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
		lua_pop(l, 1);
	}

	if (file) {
		p->Sprite = CGraphic::New(file, w, h);
		delete[] file;
	}
}

/**
**  Define a button style
**
**  @param l  Lua state.
*/
static int CclDefineButtonStyle(lua_State *l)
{
	const char *style;
	const char *value;
	ButtonStyle *b;

	LuaCheckArgs(l, 2);
	if (!lua_istable(l, 2)) {
		LuaError(l, "incorrect argument");
	}

	style = LuaToString(l, 1);
	b = ButtonStyleHash[style];
	if (!b) {
		b = ButtonStyleHash[style] = new ButtonStyle;
		// Set to bogus value to see if it was set later
		b->Default.TextX = b->Hover.TextX = b->Selected.TextX =
			b->Clicked.TextX = b->Disabled.TextX = 0xFFFFFF;
	}

	lua_pushnil(l);
	while (lua_next(l, 2)) {
		value = LuaToString(l, -2);
		if (!strcmp(value, "Size")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			b->Width = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			b->Height = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "Font")) {
			b->Font = CFont::Get(LuaToString(l, -1));
		} else if (!strcmp(value, "TextNormalColor")) {
			delete[] b->TextNormalColor;
			b->TextNormalColor = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "TextReverseColor")) {
			delete[] b->TextReverseColor;
			b->TextReverseColor = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "TextPos")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			b->TextX = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			b->TextY = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "TextAlign")) {
			value = LuaToString(l, -1);
			if (!strcmp(value, "Center")) {
				b->TextAlign = TextAlignCenter;
			} else if (!strcmp(value, "Right")) {
				b->TextAlign = TextAlignRight;
			} else if (!strcmp(value, "Left")) {
				b->TextAlign = TextAlignLeft;
			} else {
				LuaError(l, "Invalid text alignment: %s" _C_ value);
			}
		} else if (!strcmp(value, "Default")) {
			ParseButtonStyleProperties(l, &b->Default);
		} else if (!strcmp(value, "Hover")) {
			ParseButtonStyleProperties(l, &b->Hover);
		} else if (!strcmp(value, "Selected")) {
			ParseButtonStyleProperties(l, &b->Selected);
		} else if (!strcmp(value, "Clicked")) {
			ParseButtonStyleProperties(l, &b->Clicked);
		} else if (!strcmp(value, "Disabled")) {
			ParseButtonStyleProperties(l, &b->Disabled);
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
		lua_pop(l, 1);
	}

	if (b->Default.TextX == 0xFFFFFF) {
		b->Default.TextX = b->TextX;
		b->Default.TextY = b->TextY;
	}
	if (b->Hover.TextX == 0xFFFFFF) {
		b->Hover.TextX = b->TextX;
		b->Hover.TextY = b->TextY;
	}
	if (b->Selected.TextX == 0xFFFFFF) {
		b->Selected.TextX = b->TextX;
		b->Selected.TextY = b->TextY;
	}
	if (b->Clicked.TextX == 0xFFFFFF) {
		b->Clicked.TextX = b->TextX;
		b->Clicked.TextY = b->TextY;
	}
	if (b->Disabled.TextX == 0xFFFFFF) {
		b->Disabled.TextX = b->TextX;
		b->Disabled.TextY = b->TextY;
	}

	if (b->Default.TextAlign == TextAlignUndefined) {
		b->Default.TextAlign = b->TextAlign;
	}
	if (b->Hover.TextAlign == TextAlignUndefined) {
		b->Hover.TextAlign = b->TextAlign;
	}
	if (b->Selected.TextAlign == TextAlignUndefined) {
		b->Selected.TextAlign = b->TextAlign;
	}
	if (b->Clicked.TextAlign == TextAlignUndefined) {
		b->Clicked.TextAlign = b->TextAlign;
	}
	if (b->Disabled.TextAlign == TextAlignUndefined) {
		b->Disabled.TextAlign = b->TextAlign;
	}

	return 0;
}

/**
**  Define a checkbox style
**
**  @param l  Lua state.
*/
static int CclDefineCheckboxStyle(lua_State *l)
{
	const char *style;
	const char *value;
	CheckboxStyle *c;

	LuaCheckArgs(l, 2);
	if (!lua_istable(l, 2)) {
		LuaError(l, "incorrect argument");
	}

	style = LuaToString(l, 1);
	c = CheckboxStyleHash[style];
	if (!c) {
		c = CheckboxStyleHash[style] = new CheckboxStyle;
		// Set to bogus value to see if it was set later
		c->Default.TextX = c->Hover.TextX = c->Selected.TextX =
			c->Clicked.TextX = c->Disabled.TextX =
			c->Checked.TextX = c->CheckedHover.TextX = c->CheckedSelected.TextX =
			c->CheckedClicked.TextX = c->CheckedDisabled.TextX = 0xFFFFFF;
	}

	lua_pushnil(l);
	while (lua_next(l, 2)) {
		value = LuaToString(l, -2);
		if (!strcmp(value, "Size")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			c->Width = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			c->Height = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "Font")) {
			c->Font = CFont::Get(LuaToString(l, -1));
		} else if (!strcmp(value, "TextNormalColor")) {
			delete[] c->TextNormalColor;
			c->TextNormalColor = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "TextReverseColor")) {
			delete[] c->TextReverseColor;
			c->TextReverseColor = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "TextPos")) {
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			c->TextX = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			c->TextY = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "TextAlign")) {
			value = LuaToString(l, -1);
			if (!strcmp(value, "Center")) {
				c->TextAlign = TextAlignCenter;
			} else if (!strcmp(value, "Right")) {
				c->TextAlign = TextAlignRight;
			} else if (!strcmp(value, "Left")) {
				c->TextAlign = TextAlignLeft;
			} else {
				LuaError(l, "Invalid text alignment: %s" _C_ value);
			}
		} else if (!strcmp(value, "Default")) {
			ParseButtonStyleProperties(l, &c->Default);
		} else if (!strcmp(value, "Hover")) {
			ParseButtonStyleProperties(l, &c->Hover);
		} else if (!strcmp(value, "Selected")) {
			ParseButtonStyleProperties(l, &c->Selected);
		} else if (!strcmp(value, "Clicked")) {
			ParseButtonStyleProperties(l, &c->Clicked);
		} else if (!strcmp(value, "Disabled")) {
			ParseButtonStyleProperties(l, &c->Disabled);
		} else if (!strcmp(value, "Checked")) {
			ParseButtonStyleProperties(l, &c->Checked);
		} else if (!strcmp(value, "CheckedHover")) {
			ParseButtonStyleProperties(l, &c->CheckedHover);
		} else if (!strcmp(value, "CheckedSelected")) {
			ParseButtonStyleProperties(l, &c->CheckedSelected);
		} else if (!strcmp(value, "CheckedClicked")) {
			ParseButtonStyleProperties(l, &c->CheckedClicked);
		} else if (!strcmp(value, "CheckedDisabled")) {
			ParseButtonStyleProperties(l, &c->CheckedDisabled);
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
		lua_pop(l, 1);
	}

	if (c->Default.TextX == 0xFFFFFF) {
		c->Default.TextX = c->TextX;
		c->Default.TextY = c->TextY;
	}
	if (c->Hover.TextX == 0xFFFFFF) {
		c->Hover.TextX = c->TextX;
		c->Hover.TextY = c->TextY;
	}
	if (c->Selected.TextX == 0xFFFFFF) {
		c->Selected.TextX = c->TextX;
		c->Selected.TextY = c->TextY;
	}
	if (c->Clicked.TextX == 0xFFFFFF) {
		c->Clicked.TextX = c->TextX;
		c->Clicked.TextY = c->TextY;
	}
	if (c->Disabled.TextX == 0xFFFFFF) {
		c->Disabled.TextX = c->TextX;
		c->Disabled.TextY = c->TextY;
	}
	if (c->Checked.TextX == 0xFFFFFF) {
		c->Checked.TextX = c->TextX;
		c->Checked.TextY = c->TextY;
	}
	if (c->CheckedHover.TextX == 0xFFFFFF) {
		c->CheckedHover.TextX = c->TextX;
		c->CheckedHover.TextY = c->TextY;
	}
	if (c->CheckedSelected.TextX == 0xFFFFFF) {
		c->CheckedSelected.TextX = c->TextX;
		c->CheckedSelected.TextY = c->TextY;
	}
	if (c->CheckedClicked.TextX == 0xFFFFFF) {
		c->CheckedClicked.TextX = c->TextX;
		c->CheckedClicked.TextY = c->TextY;
	}
	if (c->CheckedDisabled.TextX == 0xFFFFFF) {
		c->CheckedDisabled.TextX = c->TextX;
		c->CheckedDisabled.TextY = c->TextY;
	}

	if (c->Default.TextAlign == TextAlignUndefined) {
		c->Default.TextAlign = c->TextAlign;
	}
	if (c->Hover.TextAlign == TextAlignUndefined) {
		c->Hover.TextAlign = c->TextAlign;
	}
	if (c->Selected.TextAlign == TextAlignUndefined) {
		c->Selected.TextAlign = c->TextAlign;
	}
	if (c->Clicked.TextAlign == TextAlignUndefined) {
		c->Clicked.TextAlign = c->TextAlign;
	}
	if (c->Disabled.TextAlign == TextAlignUndefined) {
		c->Disabled.TextAlign = c->TextAlign;
	}
	if (c->Checked.TextAlign == TextAlignUndefined) {
		c->Checked.TextAlign = c->TextAlign;
	}
	if (c->CheckedHover.TextAlign == TextAlignUndefined) {
		c->CheckedHover.TextAlign = c->TextAlign;
	}
	if (c->CheckedSelected.TextAlign == TextAlignUndefined) {
		c->CheckedSelected.TextAlign = c->TextAlign;
	}
	if (c->CheckedClicked.TextAlign == TextAlignUndefined) {
		c->CheckedClicked.TextAlign = c->TextAlign;
	}
	if (c->CheckedDisabled.TextAlign == TextAlignUndefined) {
		c->CheckedDisabled.TextAlign = c->TextAlign;
	}

	return 0;
}

/**
**  Free Menu content.
**
**  @param menu  menu to free.
*/
static void FreeMenu(Menu *menu)
{
	int i;

	if (menu == NULL) {
		return;
	}
	delete[] menu->Panel;
	CGraphic::Free(menu->BackgroundG);
	for (i = 0; i < menu->NumItems; ++i) {
		switch (menu->Items[i].MiType) {
			case MiTypeText:
				FreeStringDesc(menu->Items[i].D.Text.text);
				delete menu->Items[i].D.Text.text;
				delete[] menu->Items[i].D.Text.normalcolor;
				delete[] menu->Items[i].D.Text.normalcolor;
				break;
			case MiTypeButton:
				delete[] menu->Items[i].D.Button.Text;
				break;
			case MiTypePulldown: {
				int j;

				j = menu->Items[i].D.Pulldown.noptions - 1;
				for (; j >= 0; --j) {
					delete[] menu->Items[i].D.Pulldown.options[j];
				}
				delete[] menu->Items[i].D.Pulldown.options;
				break;
			}
			default:
				break;
		}
		delete[] menu->Items[i].Id;
	}
	delete[] menu->Items;
	memset(menu, 0, sizeof(*menu));
}

/**
**  Define a menu
**
**  @param l  Lua state.
*/
static int CclDefineMenu(lua_State *l)
{
	const char *value;
	Menu *menu;
	Menu item;
	const char *name;
	void *func;
	int args;
	int j;

	name = NULL;
	if (Video.Width) {
		UI.Offset640X = (Video.Width - 640) / 2;
		UI.Offset480Y = (Video.Height - 480) / 2;
	}

	//
	// Parse the arguments, already the new tagged format.
	//
	memset(&item, 0, sizeof(Menu));

	args = lua_gettop(l);
	for (j = 0; j < args; ++j) {
		value = LuaToString(l, j + 1);
		++j;
		if (!strcmp(value, "geometry")) {
			if (!lua_istable(l, j + 1) || luaL_getn(l, j + 1) != 4) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, j + 1, 1);
			item.X = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, j + 1, 2);
			item.Y = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, j + 1, 3);
			item.Width = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, j + 1, 4);
			item.Height = LuaToNumber(l, -1);
			lua_pop(l, 1);

		} else if (!strcmp(value, "name")) {
			name = LuaToString(l, j + 1);
		} else if (!strcmp(value, "panel")) {
			if (strcmp(LuaToString(l, j + 1), "none")) {
				item.Panel = new_strdup(LuaToString(l, j + 1));
			}
		} else if (!strcmp(value, "background")) {
			item.BackgroundG = CGraphic::New(LuaToString(l, j + 1));
		} else if (!strcmp(value, "default")) {
			item.DefSel = LuaToNumber(l, j + 1);
/*
		} else if (!strcmp(value, "nitems")) {
			item.nitems = LuaToNumber(l, j + 1);
*/
		} else if (!strcmp(value, "init")) {
			value = LuaToString(l, j + 1);
			func = MenuFuncHash[value];
			if (func != NULL) {
				item.InitFunc = (InitFuncType)func;
			} else {
				LuaError(l, "Can't find function: %s" _C_ value);
			}
		} else if (!strcmp(value, "exit")) {
			value = LuaToString(l, j + 1);
			func = MenuFuncHash[value];
			if (func != NULL) {
				item.ExitFunc = (ExitFuncType)func;
			} else {
				LuaError(l, "Can't find function: %s" _C_ value);
			}
		} else if (!strcmp(value, "netaction")) {
			value = LuaToString(l, j + 1);
			func = MenuFuncHash[value];
			if (func != NULL) {
				item.NetAction = (NetActionType)func;
			} else {
				LuaError(l, "Can't find function: %s" _C_ value);
			}
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
	}

	if (name) {
		menu = FindMenu(name);
		if (!menu) {
			menu = (Menu *)malloc(sizeof(Menu));
			MenuMap[name] = menu;
		} else {
			FreeMenu(menu);
		}
		memcpy(menu, &item, sizeof(Menu));
		//move the buttons for different resolutions..
		if (Video.Width != 640) {
			//printf("Video.Width = %d\n", Video.Width);
			menu->X += UI.Offset640X;
			menu->Y += UI.Offset480Y;
		}
	} else {
		fprintf(stderr, "Name of menu is missed, skip definition\n");
	}

	return 0;
}

/**
**  Convert a key string into the keypress code.
**
**  @param l      The lua state for reporting errors. 
**  @param value  Text value of the key to be decoded.
**
**  @return  The value of the hotkey as an integer.
*/
static int scm2hotkey(lua_State *l, const char *value)
{
	int len;
	int key;
	int f;

	key = 0;
	len = strlen(value);

	if (len == 0) {
		key = 0;
	} else if (len == 1) {
		key = value[0];
	} else if (!strcmp(value, "esc")) {
		key = 27;
	} else if (value[0] == 'f' && len > 1 && len < 4) {
		f = atoi(value + 1);
		if (f > 0 && f < 13) {
			key = SDLK_F1 + f - 1; // if key-order in include/interface.h is linear
		} else {
			LuaError(l, "Unknown key: %s" _C_ value);
		}
	} else {
		LuaError(l, "Unknown key %s" _C_ value);
	}
	return key;
}

/**
**  Convert a text string to a slider style
**
**  @param l      The lua state for reporting errors.
**  @param value  The text value of the type of slider.
**
**  @return  The value of the slider as an integer.
*/
static int scm2style(lua_State *l, const char *value)
{
	LuaError(l, "Unsupported style: %s" _C_ value);
	return 0;
}

/**
**  Parse menu item text
*/
static void ParseMenuItemText(lua_State *l, Menuitem *item, int j)
{
	const char *value;
	int subargs;
	int k;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeText;
	item->D.Text.text = NULL;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;
		if (!strcmp(value, "align")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			if (!strcmp(value, "left")) {
				item->D.Text.Align = TextAlignLeft;
			} else if (!strcmp(value, "right")) {
				item->D.Text.Align = TextAlignRight;
			} else if (!strcmp(value, "center")) {
				item->D.Text.Align = TextAlignCenter;
			}
		} else if (!strcmp(value, "caption")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (lua_isnil(l, -1)) {
				item->D.Text.text = 0;
				lua_pop(l, 1);
			} else {
				item->D.Text.text = CclParseStringDesc(l);
			}
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Text.action = (MenuitemTextActionType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Text.action = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "color-normal")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Text.normalcolor = new_strdup(LuaToString(l, -1));
			lua_pop(l, 1);
		} else if (!strcmp(value, "color-reverse")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Text.reversecolor = new_strdup(LuaToString(l, -1));
			lua_pop(l, 1);
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Add a Lua handler
**  FIXME: when should these be freed?
*/
int AddHandler(lua_State *l)
{
	lua_pushstring(l, "_handlers_");
	lua_gettable(l, LUA_GLOBALSINDEX);
	if (lua_isnil(l, -1)) {
		lua_pop(l, 1);
		lua_pushstring(l, "_handlers_");
		lua_newtable(l);
		lua_settable(l, LUA_GLOBALSINDEX);
		lua_pushstring(l, "_handlers_");
		lua_gettable(l, LUA_GLOBALSINDEX);
	}
	lua_pushvalue(l, -2);
	lua_rawseti(l, -2, HandleCount);
	lua_pop(l, 1);

	return HandleCount++;
}

/**
**  Call a Lua handler
*/
void CallHandler(unsigned int handle, int value)
{
	lua_pushstring(Lua, "_handlers_");
	lua_gettable(Lua, LUA_GLOBALSINDEX);
	lua_rawgeti(Lua, -1, handle);
	lua_pushnumber(Lua, value);
	LuaCall(1, 1);
	lua_pop(Lua, 1);
}

/**
**  Parse menu item button
*/
static void ParseMenuItemButton(lua_State *l, Menuitem *item, int j)
{
	int subargs;
	int k;
	const char *value;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeButton;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;

		if (!strcmp(value, "caption")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				item->D.Button.Text = new_strdup(lua_tostring(l, -1));
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Button.Text = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "hotkey")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.Button.HotKey = scm2hotkey(l, value);
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isfunction(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Button.Handler = (MenuitemButtonHandlerType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else if (lua_isfunction(l, -1)) {
				item->LuaHandle = AddHandler(l);
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Button.Handler = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "style")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.Button.Style = FindButtonStyle(value);
			if (!item->D.Button.Style) {
				LuaError(l, "Invalid button style: %s" _C_ value);
			}
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Parse menu item pulldown
*/
static void ParseMenuItemPulldown(lua_State *l, Menuitem *item, int j)
{
	int subargs;
	int k;
	const char *value;
	char *s1;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypePulldown;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;
		if (!strcmp(value, "size")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			item->D.Pulldown.xsize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			item->D.Pulldown.ysize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "options")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}

			if (!lua_isnil(l, -1)) {
				int subsubargs;
				int subk;

				subsubargs = luaL_getn(l, -1);
				item->D.Pulldown.noptions = subsubargs;
				delete[] item->D.Pulldown.options;
				item->D.Pulldown.options = new char *[subsubargs];
				for (subk = 0; subk < subsubargs; ++subk) {
					lua_rawgeti(l, -1, subk + 1);
					s1 = new_strdup(LuaToString(l, -1));
					lua_pop(l, 1);
					item->D.Pulldown.options[subk] = s1;
				}
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isfunction(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Pulldown.action = (MenuitemPulldownActionType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else if (lua_isfunction(l, -1)) {
				item->LuaHandle = AddHandler(l);
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Pulldown.action = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "style")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.Pulldown.button = scm2buttonid(l, value);
		} else if (!strcmp(value, "state")) {
			// FIXME: need generic way to set menuitem as disabled
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			if (!strcmp(value, "passive")) {
				item->Flags |= MI_FLAGS_DISABLED;
			} else {
				LuaError(l, "Unsupported property: %s" _C_ value);
			}
		} else if (!strcmp(value, "default")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Pulldown.defopt = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "current")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Pulldown.curopt = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Parse menu item listbox
*/
static void ParseMenuItemListbox(lua_State *l, Menuitem *item, int j)
{
	int subargs;
	int k;
	const char *value;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeListbox;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;
		if (!strcmp(value, "size")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			item->D.Listbox.xsize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			item->D.Listbox.ysize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isfunction(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Listbox.action = (MenuitemListboxActionType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else if (lua_isfunction(l, -1)) {
				item->LuaHandle = AddHandler(l);
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Listbox.action = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "handler")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Listbox.handler = (MenuitemListboxHandlerType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Listbox.handler = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "retopt")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Listbox.retrieveopt = (MenuitemListboxRetrieveType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Listbox.retrieveopt = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "style")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.Listbox.button = scm2buttonid(l, value);
		} else if (!strcmp(value, "default")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Listbox.defopt = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "startline")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Listbox.startline = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "nlines")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Listbox.nlines = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "current")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Listbox.curopt = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Parse menu item vslider
*/
static void ParseMenuItemVSlider(lua_State *l, Menuitem *item, int j)
{
	int subargs;
	int k;
	const char *value;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeVslider;
	item->D.VSlider.defper = -1;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;
		if (!strcmp(value, "size")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			item->D.VSlider.xsize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			item->D.VSlider.ysize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "flags")) {
			int subk;
			int subsubargs;

			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument");
			}

			subsubargs = luaL_getn(l, -1);
			for (subk = 0; subk < subsubargs; ++subk) {
				lua_rawgeti(l, -1, subk + 1);
				value = LuaToString(l, -1);
				lua_pop(l, 1);

				if (!strcmp(value, "up")) {
					item->D.VSlider.cflags |= MI_CFLAGS_UP;
				} else if (!strcmp(value, "down")) {
					item->D.VSlider.cflags |= MI_CFLAGS_DOWN;
				} else if (!strcmp(value, "left")) {
					item->D.VSlider.cflags |= MI_CFLAGS_LEFT;
				} else if (!strcmp(value, "right")) {
					item->D.VSlider.cflags |= MI_CFLAGS_RIGHT;
				} else if (!strcmp(value, "knob")) {
					item->D.VSlider.cflags |= MI_CFLAGS_KNOB;
				} else if (!strcmp(value, "cont")) {
					item->D.VSlider.cflags |= MI_CFLAGS_CONT;
				} else {
					LuaError(l, "Unknown flag: %s" _C_ value);
				}
			}
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isfunction(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.VSlider.action = (MenuitemVSliderActionType)func;
				} else {
					lua_pushfstring(l, "Can't find function: %s", value);
				}
			} else if (lua_isfunction(l, -1)) {
				item->LuaHandle = AddHandler(l);
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.VSlider.action = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "handler")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.VSlider.handler = (MenuitemVSliderHandlerType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.VSlider.handler = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "default")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.VSlider.defper = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "current")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.VSlider.percent = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "style")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.VSlider.style = scm2style(l, value);
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Parse menu item hslider
*/
static void ParseMenuItemHSlider(lua_State *l, Menuitem *item, int j)
{
	int subargs;
	int k;
	const char *value;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeHslider;
	item->D.HSlider.defper = -1;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;
		if (!strcmp(value, "size")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			item->D.HSlider.xsize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			item->D.HSlider.ysize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "flags")) {
			int subk;
			int subsubargs;

			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument");
			}

			subsubargs = luaL_getn(l, -1);
			for (subk = 0; subk < subsubargs; ++subk) {
				lua_rawgeti(l, -1, subk + 1);
				value = LuaToString(l, -1);
				lua_pop(l, 1);

				if (!strcmp(value, "up")) {
					item->D.HSlider.cflags |= MI_CFLAGS_UP;
				} else if (!strcmp(value, "down")) {
					item->D.HSlider.cflags |= MI_CFLAGS_DOWN;
				} else if (!strcmp(value, "left")) {
					item->D.HSlider.cflags |= MI_CFLAGS_LEFT;
				} else if (!strcmp(value, "right")) {
					item->D.HSlider.cflags |= MI_CFLAGS_RIGHT;
				} else if (!strcmp(value, "knob")) {
					item->D.HSlider.cflags |= MI_CFLAGS_KNOB;
				} else if (!strcmp(value, "cont")) {
					item->D.HSlider.cflags |= MI_CFLAGS_CONT;
				} else {
					LuaError(l, "Unknown flag: %s" _C_ value);
				}
			}
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isfunction(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.HSlider.action = (MenuitemHSliderActionType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else if (lua_isfunction(l, -1)) {
				item->LuaHandle = AddHandler(l);
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.HSlider.action = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "handler")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.HSlider.handler = (MenuitemHSliderHandlerType)func;
				} else {
					LuaError(l, "Can't find function: %s" _C_ value);
				}
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.HSlider.handler = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "default")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.HSlider.defper = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "current")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.HSlider.percent = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "style")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.HSlider.style = scm2style(l, value);
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Parse menu item drawfunc
*/
static void ParseMenuItemDrawFunc(lua_State *l, Menuitem *item, int j)
{
	const char *value;
	void *func;

	if (!lua_isstring(l, j + 1) && !lua_isnil(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeDrawfunc;

	if (lua_isstring(l, j + 1)) {
		value = lua_tostring(l, j + 1);
		func = MenuFuncHash[value];
		if (func != NULL) {
			item->D.DrawFunc.draw = (MenuitemDrawfuncDrawType)func;
		} else {
			LuaError(l, "Can't find function: %s" _C_ value);
		}
	} else {
		item->D.DrawFunc.draw = NULL;
	}
}

/**
**  Parse menu item input
*/
static void ParseMenuItemInput(lua_State *l, Menuitem *item, int j)
{
	int subargs;
	int k;
	const char *value;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeInput;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;
		if (!strcmp(value, "size")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, -1, 1);
			item->D.Input.xsize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, -1, 2);
			item->D.Input.ysize = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isfunction(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Input.action = (MenuitemInputActionType)func;
				} else {
					lua_pushfstring(l, "Can't find function: %s", value);
				}
			} else if (lua_isfunction(l, -1)) {
				item->LuaHandle = AddHandler(l);
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Input.action = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "style")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.Input.button = scm2buttonid(l, value);
		} else if (!strcmp(value, "maxch")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Input.maxch = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "color-normal")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Input.normalcolor = new_strdup(LuaToString(l, -1));
			lua_pop(l, 1);
		} else if (!strcmp(value, "color-reverse")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Input.reversecolor = new_strdup(LuaToString(l, -1));
			lua_pop(l, 1);
		} else if (!strcmp(value, "password")) {
			item->D.Input.iflags = 1;
			--k;
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Parse menu item checkbox
*/
static void ParseMenuItemCheckbox(lua_State *l, Menuitem *item, int j)
{
	int subargs;
	int k;
	const char *value;
	void *func;

	if (!lua_istable(l, j + 1)) {
		LuaError(l, "incorrect argument");
	}
	item->MiType = MiTypeCheckbox;

	subargs = luaL_getn(l, j + 1);
	for (k = 0; k < subargs; ++k) {
		lua_rawgeti(l, j + 1, k + 1);
		value = LuaToString(l, -1);
		lua_pop(l, 1);
		++k;
		if (!strcmp(value, "state")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			if (!strcmp(value, "unchecked")) {
				item->D.Checkbox.Checked = 0;
			} else if (!strcmp(value, "checked")) {
				item->D.Checkbox.Checked = 1;
			}
		} else if (!strcmp(value, "func")) {
			lua_rawgeti(l, j + 1, k + 1);
			if (!lua_isstring(l, -1) && !lua_isfunction(l, -1) && !lua_isnil(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isstring(l, -1)) {
				value = lua_tostring(l, -1);
				func = MenuFuncHash[value];
				if (func != NULL) {
					item->D.Checkbox.Action = (MenuitemCheckboxActionType)func;
				} else {
					lua_pushfstring(l, "Can't find function: %s", value);
				}
			} else if (lua_isfunction(l, -1)) {
				item->LuaHandle = AddHandler(l);
			} else {
				lua_pushnumber(l, 0);
				lua_rawseti(l, j + 1, k + 1);
				subargs = luaL_getn(l, j + 1);
				item->D.Checkbox.Action = NULL;
			}
			lua_pop(l, 1);
		} else if (!strcmp(value, "style")) {
			lua_rawgeti(l, j + 1, k + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			item->D.Checkbox.Style = FindCheckboxStyle(value);
			if (!item->D.Checkbox.Style) {
				LuaError(l, "Invalid button style: %s" _C_ value);
			}
		} else if (!strcmp(value, "text")) {
			lua_rawgeti(l, j + 1, k + 1);
			item->D.Checkbox.Text = new_strdup(LuaToString(l, -1));
			lua_pop(l, 1);
		} else {
			LuaError(l, "Unsupported property: %s" _C_ value);
		}
	}
}

/**
**  Define a menu item
*/
static int CclDefineMenuItem(lua_State *l)
{
	const char *value;
	char *name;
	Menuitem *item;
	Menu *menu;
	int args;
	int subargs;
	int j;
	int k;

	name = NULL;
	item = (Menuitem *)calloc(1, sizeof(Menuitem));

	//
	// Parse the arguments, already the new tagged format.
	//
	args = lua_gettop(l);
	for (j = 0; j < args; ++j) {
		value = LuaToString(l, j + 1);
		++j;
		if (!strcmp(value, "pos")) {
			if (!lua_istable(l, j + 1) || luaL_getn(l, j + 1) != 2) {
				LuaError(l, "incorrect argument");
			}
			lua_rawgeti(l, j + 1, 1);
			item->XOfs = LuaToNumber(l, -1);
			lua_pop(l, 1);
			lua_rawgeti(l, j + 1, 2);
			item->YOfs = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (!strcmp(value, "menu")) {
			name = new_strdup(LuaToString(l, j + 1));
		} else if (!strcmp(value, "flags")) {
			if (!lua_istable(l, j + 1)) {
				LuaError(l, "incorrect argument");
			}
			subargs = luaL_getn(l, j + 1);
			for (k = 0; k < subargs; ++k) {
				lua_rawgeti(l, j + 1, k + 1);
				value = LuaToString(l, -1);
				lua_pop(l, 1);

				if (!strcmp(value, "active")) {
					item->Flags |= MI_FLAGS_ACTIVE;
				} else if (!strcmp(value, "clicked")) {
					item->Flags |= MI_FLAGS_CLICKED;
				} else if (!strcmp(value, "selected")) {
					item->Flags |= MI_FLAGS_SELECTED;
				} else if (!strcmp(value, "disabled")) {
					item->Flags |= MI_FLAGS_DISABLED;
				} else if (!strcmp(value, "invisible")) {
					item->Flags |= MI_FLAGS_INVISIBLE;
				} else {
					LuaError(l, "Unknown flag: %s" _C_ value);
				}
			}
		} else if (!strcmp(value, "id")) {
			item->Id = new_strdup(LuaToString(l, j + 1));
		} else if (!strcmp(value, "font")) {
			item->Font = CFont::Get(LuaToString(l, j + 1));
/* Menu types */
		} else if (!item->MiType) {
			if (!strcmp(value, "text")) {
				ParseMenuItemText(l, item, j);
			} else if (!strcmp(value, "button")) {
				ParseMenuItemButton(l, item, j);
			} else if (!strcmp(value, "pulldown")) {
				ParseMenuItemPulldown(l, item, j);
			} else if (!strcmp(value, "listbox")) {
				ParseMenuItemListbox(l, item, j);
			} else if (!strcmp(value, "vslider")) {
				ParseMenuItemVSlider(l, item, j);
			} else if (!strcmp(value, "hslider")) {
				ParseMenuItemHSlider(l, item, j);
			} else if (!strcmp(value, "drawfunc")) {
				ParseMenuItemDrawFunc(l, item, j);
			} else if (!strcmp(value, "input")) {
				ParseMenuItemInput(l, item, j);
			} else if (!strcmp(value, "checkbox")) {
				ParseMenuItemCheckbox(l, item, j);
			} else {
				LuaError(l, "Unsupported tag: %s" _C_ value);
			}
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
	}

	if ((menu = MenuMap[name])) {
		Menuitem *newitems = new Menuitem[menu->NumItems + 1];
		if (menu->Items) {
			memcpy(newitems, menu->Items, menu->NumItems * sizeof(Menuitem));
			delete[] menu->Items;
		}
		menu->Items = newitems;
		item->Menu = menu;
		memcpy(menu->Items + menu->NumItems, item, sizeof(Menuitem));
		menu->NumItems++;
	}
	delete[] name;
	free(item);

	return 0;
}

/**
**  Define menu graphics
**
**  @param l  Lua state.
*/
static int CclDefineMenuGraphics(lua_State *l)
{
	int i;
	int j;
	int t;
	int tables;
	int args;
	const char *value;
	char *file;
	int w;
	int h;

	LuaCheckArgs(l, 1);
	if (!lua_istable(l, 1)) {
		LuaError(l, "incorrect argument");
	}

	tables = luaL_getn(l, 1);
	i = 0;

	for (t = 0; t < tables; ++t) {
		lua_rawgeti(l, 1, t + 1);
		args = luaL_getn(l, -1);
		if (!lua_istable(l, -1)) {
			LuaError(l, "incorrect argument");
		}
		file = NULL;
		w = h = 0;
		for (j = 0; j < args; ++j) {
			lua_rawgeti(l, -1, j + 1);
			value = LuaToString(l, -1);
			lua_pop(l, 1);
			if (!strcmp(value, "file")) {
				++j;
				lua_rawgeti(l, -1, j + 1);
				file = new_strdup(LuaToString(l, -1));
				lua_pop(l, 1);
			} else if (!strcmp(value, "size")) {
				++j;
				lua_rawgeti(l, -1, j + 1);
				if (!lua_istable(l, -1) || luaL_getn(l, -1) != 2) {
					LuaError(l, "incorrect argument");
				}
				lua_rawgeti(l, -1, 1);
				w = LuaToNumber(l, -1);
				lua_pop(l, 1);
				lua_rawgeti(l, -1, 2);
				h = LuaToNumber(l, -1);
				lua_pop(l, 1);
				lua_pop(l, 1);
			} else {
				LuaError(l, "incorrect argument");
			}
		}
		MenuButtonGraphics[i] = CGraphic::New(file, w, h);
		delete[] file;
		++i;
		lua_pop(l, 1);
	}

	return 0;
}

/**
**  Define a button.
**
**  @param l  Lua state.
*/
static int CclDefineButton(lua_State *l)
{
	char buf[64];
	const char *value;
	char *s1;
	const char *s2;
	ButtonAction ba;

	LuaCheckArgs(l, 1);
	if (!lua_istable(l, 1)) {
		LuaError(l, "incorrect argument");
	}

	//
	// Parse the arguments
	//
	lua_pushnil(l);
	while (lua_next(l, 1)) {
		value = LuaToString(l, -2);
		if (!strcmp(value, "Pos")) {
			ba.Pos = LuaToNumber(l, -1);
		} else if (!strcmp(value, "Level")) {
			ba.Level = LuaToNumber(l, -1);
		} else if (!strcmp(value, "Icon")) {
			ba.Icon.Name = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "Action")) {
			value = LuaToString(l, -1);
			if (!strcmp(value, "move")) {
				ba.Action = ButtonMove;
			} else if (!strcmp(value, "stop")) {
				ba.Action = ButtonStop;
			} else if (!strcmp(value, "attack")) {
				ba.Action = ButtonAttack;
			} else if (!strcmp(value, "repair")) {
				ba.Action = ButtonRepair;
			} else if (!strcmp(value, "harvest")) {
				ba.Action = ButtonHarvest;
			} else if (!strcmp(value, "button")) {
				ba.Action = ButtonButton;
			} else if (!strcmp(value, "build")) {
				ba.Action = ButtonBuild;
			} else if (!strcmp(value, "train-unit")) {
				ba.Action = ButtonTrain;
			} else if (!strcmp(value, "patrol")) {
				ba.Action = ButtonPatrol;
			} else if (!strcmp(value, "stand-ground")) {
				ba.Action = ButtonStandGround;
			} else if (!strcmp(value, "attack-ground")) {
				ba.Action = ButtonAttackGround;
			} else if (!strcmp(value, "return-goods")) {
				ba.Action = ButtonReturn;
			} else if (!strcmp(value, "cast-spell")) {
				ba.Action = ButtonSpellCast;
			} else if (!strcmp(value, "research")) {
				ba.Action = ButtonResearch;
			} else if (!strcmp(value, "upgrade-to")) {
				ba.Action = ButtonUpgradeTo;
			} else if (!strcmp(value, "unload")) {
				ba.Action = ButtonUnload;
			} else if (!strcmp(value, "cancel")) {
				ba.Action = ButtonCancel;
			} else if (!strcmp(value, "cancel-upgrade")) {
				ba.Action = ButtonCancelUpgrade;
			} else if (!strcmp(value, "cancel-train-unit")) {
				ba.Action = ButtonCancelTrain;
			} else if (!strcmp(value, "cancel-build")) {
				ba.Action = ButtonCancelBuild;
			} else {
				LuaError(l, "Unsupported button action: %s" _C_ value);
			}
		} else if (!strcmp(value, "Value")) {
			if (!lua_isnumber(l, -1) && !lua_isstring(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			if (lua_isnumber(l, -1)) {
				sprintf(buf, "%ld", (long int)lua_tonumber(l, -1));
				s1 = new_strdup(buf);
			} else {
				s1 = new_strdup(lua_tostring(l, -1));
			}
			ba.ValueStr = s1;
		} else if (!strcmp(value, "Allowed")) {
			value = LuaToString(l, -1);
			if (!strcmp(value, "check-true")) {
				ba.Allowed = ButtonCheckTrue;
			} else if (!strcmp(value, "check-false")) {
				ba.Allowed = ButtonCheckFalse;
			} else if (!strcmp(value, "check-upgrade")) {
				ba.Allowed = ButtonCheckUpgrade;
			} else if (!strcmp(value, "check-units-or")) {
				ba.Allowed = ButtonCheckUnitsOr;
			} else if (!strcmp(value, "check-units-and")) {
				ba.Allowed = ButtonCheckUnitsAnd;
			} else if (!strcmp(value, "check-network")) {
				ba.Allowed = ButtonCheckNetwork;
			} else if (!strcmp(value, "check-no-network")) {
				ba.Allowed = ButtonCheckNoNetwork;
			} else if (!strcmp(value, "check-no-work")) {
				ba.Allowed = ButtonCheckNoWork;
			} else if (!strcmp(value, "check-no-research")) {
				ba.Allowed = ButtonCheckNoResearch;
			} else if (!strcmp(value, "check-attack")) {
				ba.Allowed = ButtonCheckAttack;
			} else if (!strcmp(value, "check-upgrade-to")) {
				ba.Allowed = ButtonCheckUpgradeTo;
			} else if (!strcmp(value, "check-research")) {
				ba.Allowed = ButtonCheckResearch;
			} else if (!strcmp(value, "check-single-research")) {
				ba.Allowed = ButtonCheckSingleResearch;
			} else {
				LuaError(l, "Unsupported action: %s" _C_ value);
			}
		} else if (!strcmp(value, "AllowArg")) {
			int subargs;
			int k;

			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			s1 = new_strdup("");
			subargs = luaL_getn(l, -1);
			for (k = 0; k < subargs; ++k) {
				lua_rawgeti(l, -1, k + 1);
				s2 = LuaToString(l, -1);
				lua_pop(l, 1);
				char *news1 = new char[strlen(s1) + strlen(s2) + 2];
				strcpy(news1, s1);
				strcat(news1, s2);
				delete[] s1;
				s1 = news1;
				if (k != subargs - 1) {
					strcat(s1, ",");
				}
			}
			ba.AllowStr = s1;
		} else if (!strcmp(value, "Key")) {
			ba.Key = *LuaToString(l, -1);
		} else if (!strcmp(value, "Hint")) {
			ba.Hint = new_strdup(LuaToString(l, -1));
		} else if (!strcmp(value, "ForUnit")) {
			int subargs;
			int k;

			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			// FIXME: ba.UnitMask shouldn't be a string
			s1 = new_strdup(",");
			subargs = luaL_getn(l, -1);
			for (k = 0; k < subargs; ++k) {
				lua_rawgeti(l, -1, k + 1);
				s2 = LuaToString(l, -1);
				lua_pop(l, 1);
				char *news1 = new char[strlen(s1) + strlen(s2) + 2];
				strcpy(news1, s1);
				strcat(news1, s2);
				strcat(news1, ",");
				delete[] s1;
				s1 = news1;
			}
			ba.UnitMask = s1;
			if (!strncmp(ba.UnitMask, ",*,", 3)) {
				delete[] ba.UnitMask;
				ba.UnitMask = new_strdup("*");
			}
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
		lua_pop(l, 1);
	}
	AddButton(ba.Pos, ba.Level, new_strdup(ba.Icon.Name), ba.Action, ba.ValueStr,
		ba.Allowed, ba.AllowStr, ba.Key, ba.Hint, ba.UnitMask);
	delete[] ba.ValueStr;
	delete[] ba.AllowStr;
	delete[] ba.Hint;
	delete[] ba.UnitMask;

	return 0;
}

/**
**  Run the set-selection-changed-hook.
*/
void SelectionChanged(void)
{
	// We Changed out selection, anything pending buttonwise must be cleared
	UI.StatusLine.Clear();
	ClearCosts();
	CurrentButtonLevel = 0;
	UI.ButtonPanel.Update();
	GameCursor = UI.Point.Cursor;
	CursorBuilding = NULL;
	CursorState = CursorStatePoint;
	UI.ButtonPanel.Update();
}

/**
**  The selected unit has been altered.
*/
void SelectedUnitChanged(void)
{
	UI.ButtonPanel.Update();
}

/**
**  Set selection style.
**
**  @param l  Lua state.
*/
static int CclSetSelectionStyle(lua_State *l)
{
	const char *style;

	LuaCheckArgs(l, 1);

	style = LuaToString(l, 1);
	if (!strcmp(style, "rectangle")) {
		DrawSelection = DrawSelectionRectangle;
	} else if (!strcmp(style, "alpha-rectangle")) {
		DrawSelection = DrawSelectionRectangleWithTrans;
	} else if (!strcmp(style, "circle")) {
		DrawSelection = DrawSelectionCircle;
	} else if (!strcmp(style, "alpha-circle")) {
		DrawSelection = DrawSelectionCircleWithTrans;
	} else if (!strcmp(style, "corners")) {
		DrawSelection = DrawSelectionCorners;
	} else {
		LuaError(l, "Unsupported selection style");
	}

	return 0;
}

/**
**  Add a new message.
**
**  @param l  Lua state.
*/
static int CclAddMessage(lua_State *l)
{
	LuaCheckArgs(l, 1);
	SetMessage("%s", LuaToString(l, 1));
	return 0;
}

/**
**  Reset the keystroke help array
**
**  @param l  Lua state.
*/
static int CclResetKeystrokeHelp(lua_State *l)
{
	LuaCheckArgs(l, 0);

	std::vector<KeyStrokeHelp>::iterator i;
	for (i = KeyStrokeHelps.begin(); i != KeyStrokeHelps.end(); ++i) {
		delete[] (*i).Key;
		delete[] (*i).Help;
	}
	KeyStrokeHelps.clear();

	return 0;
}

/**
**  Set the keys which are use for grouping units, helpful for other keyboards
**
**  @param l  Lua state.
*/
static int CclSetGroupKeys(lua_State *l)
{
	LuaCheckArgs(l, 1);
	if (UiGroupKeys != DefaultGroupKeys) {
		delete[] UiGroupKeys;
	}
	UiGroupKeys = new_strdup(LuaToString(l, 1));
	return 0;
}

/**
** Set basic map caracteristics.
**
**  @param l  Lua state.
*/
static int CclPresentMap(lua_State *l)
{
	LuaCheckArgs(l, 5);

	Map.Info.Description = new_strdup(LuaToString(l, 1));
	// Number of players in LuaToNumber(l, 3); // Not used yet.
	Map.Info.MapWidth = LuaToNumber(l, 3);
	Map.Info.MapHeight = LuaToNumber(l, 4);
	Map.Info.MapUID = LuaToNumber(l, 5);

	return 0;
}

/**
** Define the lua file that will build the map
**
**  @param l  Lua state.
*/
static int CclDefineMapSetup(lua_State *l)
{
	LuaCheckArgs(l, 1);
	delete[] Map.Info.Filename;
	Map.Info.Filename = new_strdup(LuaToString(l, 1));

	return 0;
}

/**
**  Add a keystroke help
**
**  @param l  Lua state.
*/
static int CclAddKeystrokeHelp(lua_State *l)
{
	KeyStrokeHelp h;

	LuaCheckArgs(l, 2);

	h.Key = new_strdup(LuaToString(l, 1));
	h.Help = new_strdup(LuaToString(l, 2));

	KeyStrokeHelps.push_back(h);

	return 0;
}

/**
**  Register CCL features for UI.
*/
void UserInterfaceCclRegister(void)
{
	lua_register(Lua, "AddMessage", CclAddMessage);

	lua_register(Lua, "SetMouseScrollSpeedDefault", CclSetMouseScrollSpeedDefault);
	lua_register(Lua, "SetMouseScrollSpeedControl", CclSetMouseScrollSpeedControl);

	lua_register(Lua, "SetClickMissile", CclSetClickMissile);
	lua_register(Lua, "SetDamageMissile", CclSetDamageMissile);

	lua_register(Lua, "SetVideoResolution", CclSetVideoResolution);
	lua_register(Lua, "GetVideoResolution", CclGetVideoResolution);
	lua_register(Lua, "SetVideoFullScreen", CclSetVideoFullScreen);
	lua_register(Lua, "GetVideoFullScreen", CclGetVideoFullScreen);

	lua_register(Lua, "SetTitleScreens", CclSetTitleScreens);
	lua_register(Lua, "SetMenuMusic", CclSetMenuMusic);

	lua_register(Lua, "ProcessMenu", CclProcessMenu);

	lua_register(Lua, "DefineCursor", CclDefineCursor);
	lua_register(Lua, "SetGameCursor", CclSetGameCursor);
	lua_register(Lua, "DefinePanelContents", CclDefinePanelContents);
	lua_register(Lua, "DefineViewports", CclDefineViewports);

	lua_register(Lua, "SetShowCommandKey", CclSetShowCommandKey);
	lua_register(Lua, "RightButtonAttacks", CclRightButtonAttacks);
	lua_register(Lua, "RightButtonMoves", CclRightButtonMoves);

	lua_register(Lua, "DefineButton", CclDefineButton);

	lua_register(Lua, "DefineButtonStyle", CclDefineButtonStyle);
	lua_register(Lua, "DefineCheckboxStyle", CclDefineCheckboxStyle);

	lua_register(Lua, "DefineMenuItem", CclDefineMenuItem);
	lua_register(Lua, "DefineMenu", CclDefineMenu);
	lua_register(Lua, "DefineMenuGraphics", CclDefineMenuGraphics);

	lua_register(Lua, "PresentMap", CclPresentMap);
	lua_register(Lua, "DefineMapSetup", CclDefineMapSetup);

	//
	// Look and feel of units
	//
	lua_register(Lua, "SetSelectionStyle", CclSetSelectionStyle);

	//
	// Keystroke helps
	//
	lua_register(Lua, "ResetKeystrokeHelp", CclResetKeystrokeHelp);
	lua_register(Lua, "AddKeystrokeHelp", CclAddKeystrokeHelp);
	lua_register(Lua, "SetGroupKeys", CclSetGroupKeys);

	InitMenuFuncHash();
}

//@}