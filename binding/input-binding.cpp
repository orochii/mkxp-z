/*
** input-binding.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 Jonas Kulla <Nyocurio@gmail.com>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "binding-util.h"
#include "exception.h"
#include "input.h"
#include "sharedstate.h"
#include "src/util.h"

#include <SDL_joystick.h>
#include <string>

RB_METHOD(inputUpdate) {
  RB_UNUSED_PARAM;

  shState->input().update();

  return Qnil;
}

static int getButtonArg(VALUE *argv) {
  int num;

  if (FIXNUM_P(*argv)) {
    num = FIX2INT(*argv);
  } else if (SYMBOL_P(*argv) && rgssVer >= 3) {
    VALUE symHash = getRbData()->buttoncodeHash;
#if RAPI_FULL > 187
    num = FIX2INT(rb_hash_lookup2(symHash, *argv, INT2FIX(Input::None)));
#else
    VALUE res = rb_hash_aref(symHash, *argv);
    if (!NIL_P(res))
      num = FIX2INT(res);
    else
      num = Input::None;
#endif
  } else {
    // FIXME: RMXP allows only few more types that
    // don't make sense (symbols in pre 3, floats)
    num = 0;
  }

  return num;
}

static int getScancodeArg(VALUE *argv) {
  const char *scancode = rb_id2name(SYM2ID(*argv));
  int code{};
  try {
    code = strToScancode[scancode];
  } catch (...) {
    rb_raise(rb_eRuntimeError, "%s is not a valid name of an SDL scancode.", scancode);
  }

  return code;
}

static int getJoyButtonArg(VALUE *argv) {
  const char *scancode = rb_id2name(SYM2ID(*argv));
  int code{};
  try {
    code = strToJoycode[scancode];
  } catch (...) {
    rb_raise(rb_eRuntimeError, "%s is not a valid name.", scancode);
  }

  return code;
}

RB_METHOD(inputPress) {
  RB_UNUSED_PARAM;

  rb_check_argc(argc, 1);

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  int num = getButtonArg(&button);

  return rb_bool_new(shState->input().isPressed(num));
}

RB_METHOD(inputTrigger) {
  RB_UNUSED_PARAM;

  rb_check_argc(argc, 1);

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  int num = getButtonArg(&button);

  return rb_bool_new(shState->input().isTriggered(num));
}

RB_METHOD(inputRepeat) {
  RB_UNUSED_PARAM;

  rb_check_argc(argc, 1);

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  int num = getButtonArg(&button);

  return rb_bool_new(shState->input().isRepeated(num));
}

RB_METHOD(inputPressEx) {
  RB_UNUSED_PARAM;

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  if (SYMBOL_P(button)) {
    int num = getScancodeArg(&button);
    return rb_bool_new(shState->input().isPressedEx(num, 0));
  }

  return rb_bool_new(shState->input().isPressedEx(NUM2INT(button), 1));
}

RB_METHOD(inputTriggerEx) {
  RB_UNUSED_PARAM;

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  if (SYMBOL_P(button)) {
    int num = getScancodeArg(&button);
    return rb_bool_new(shState->input().isTriggeredEx(num, 0));
  }

  return rb_bool_new(shState->input().isTriggeredEx(NUM2INT(button), 1));
}

RB_METHOD(inputRepeatEx) {
  RB_UNUSED_PARAM;

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  if (SYMBOL_P(button)) {
    int num = getScancodeArg(&button);
    return rb_bool_new(shState->input().isRepeatedEx(num, 0));
  }

  return rb_bool_new(shState->input().isRepeatedEx(NUM2INT(button), 1));
}

// ------------------------------------------------------------------------------------------
// JOYSTICK RAW ACCESS
// ------------------------------------------------------------------------------------------
RB_METHOD(jinputPressEx) {
  RB_UNUSED_PARAM;

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  if (SYMBOL_P(button)) {
    int num = getJoyButtonArg(&button);
    return rb_bool_new(shState->input().isJPressedEx(num, 0));
  }

  return rb_bool_new(shState->input().isJPressedEx(NUM2INT(button), 1));
}

RB_METHOD(jinputTriggerEx) {
  RB_UNUSED_PARAM;

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  if (SYMBOL_P(button)) {
    int num = getJoyButtonArg(&button);
    return rb_bool_new(shState->input().isJTriggeredEx(num, 0));
  }

  return rb_bool_new(shState->input().isJTriggeredEx(NUM2INT(button), 1));
}

RB_METHOD(jinputRepeatEx) {
  RB_UNUSED_PARAM;

  VALUE button;
  rb_scan_args(argc, argv, "1", &button);

  if (SYMBOL_P(button)) {
    int num = getJoyButtonArg(&button);
    return rb_bool_new(shState->input().isJRepeatedEx(num, 0));
  }

  return rb_bool_new(shState->input().isJRepeatedEx(NUM2INT(button), 1));
}
// ------------------------------------------------------------------------------------------

RB_METHOD(inputDir4) {
  RB_UNUSED_PARAM;

  return rb_fix_new(shState->input().dir4Value());
}

RB_METHOD(inputDir8) {
  RB_UNUSED_PARAM;

  return rb_fix_new(shState->input().dir8Value());
}

/* Non-standard extensions */
RB_METHOD(inputMouseX) {
  RB_UNUSED_PARAM;

  return rb_fix_new(shState->input().mouseX());
}

RB_METHOD(inputMouseY) {
  RB_UNUSED_PARAM;

  return rb_fix_new(shState->input().mouseY());
}

#define M_SYMBOL(x) ID2SYM(rb_intern(x))
#define POWERCASE(v, c)                                                        \
  case SDL_JOYSTICK_POWER_##c:                                                 \
    v = M_SYMBOL(#c);                                                          \
    break;

RB_METHOD(inputJoystickInfo) {
  RB_UNUSED_PARAM;

  if (!shState->input().getJoystickConnected())
    return RUBY_Qnil;

  VALUE ret = rb_hash_new();

  rb_hash_aset(ret, M_SYMBOL("name"),
               rb_str_new_cstr(shState->input().getJoystickName()));

  VALUE power;

  switch (shState->input().getJoystickPowerLevel()) {
    POWERCASE(power, MAX);
    POWERCASE(power, WIRED);
    POWERCASE(power, FULL);
    POWERCASE(power, MEDIUM);
    POWERCASE(power, LOW);
    POWERCASE(power, EMPTY);

  default:
    power = M_SYMBOL("UNKNOWN");
    break;
  }

  rb_hash_aset(ret, M_SYMBOL("power"), power);
  return ret;
}
#undef POWERCASE
#undef M_SYMBOL

RB_METHOD(inputRumble) {
  RB_UNUSED_PARAM;
  VALUE duration, strength, attack, fade;
  rb_scan_args(argc, argv, "13", &duration, &strength, &attack, &fade);

  int dur = NUM2INT(duration);
  int str = (NIL_P(strength)) ? 1 : NUM2INT(strength);
  int att = (NIL_P(attack)) ? 0 : NUM2INT(attack);
  int fad = (NIL_P(fade)) ? 0 : NUM2INT(fade);

  shState->input().rumble(dur, str, att, fad);

  return Qnil;
}

RB_METHOD(inputGetMode) {
  RB_UNUSED_PARAM;

  return rb_bool_new(shState->input().getTextInputMode());
}

RB_METHOD(inputSetMode) {
  RB_UNUSED_PARAM;

  bool mode;
  rb_get_args(argc, argv, "b", &mode RB_ARG_END);

  shState->input().setTextInputMode(mode);

  return mode;
}

RB_METHOD(inputGets) {
  RB_UNUSED_PARAM;

  VALUE ret = rb_str_new_cstr(shState->input().getText());
  shState->input().clearText();

  return ret;
}

RB_METHOD(inputLastKey) {
  RB_UNUSED_PARAM;

  return rb_fix_new(shState->input().getLastKey());
}

RB_METHOD(inputLastJoy) {
  RB_UNUSED_PARAM;

  return rb_fix_new(shState->input().getLastJoy());
}

RB_METHOD(inputGetTriggerTreshold) {
  RB_UNUSED_PARAM;

  return rb_fix_new(shState->input().getTriggerThreshold());
}

RB_METHOD(inputSetTriggerTreshold) {
  RB_UNUSED_PARAM;
  int value;
  rb_get_args(argc, argv, "i", &value RB_ARG_END);
  shState->input().setTriggerThreshold(value);
  return rb_fix_new(value);
}

RB_METHOD(inputGetClipboard) {
  RB_UNUSED_PARAM;
  VALUE ret;
  try {
    ret = rb_str_new_cstr(shState->input().getClipboardText());
  } catch (const Exception &e) {
    raiseRbExc(e);
  }
  return ret;
}

RB_METHOD(inputSetClipboard) {
  RB_UNUSED_PARAM;

  VALUE str;
  rb_scan_args(argc, argv, "1", &str);

  SafeStringValue(str);

  try {
    shState->input().setClipboardText(RSTRING_PTR(str));
  } catch (const Exception &e) {
    raiseRbExc(e);
  }
  return str;
}

struct {
  const char *str;
  Input::ButtonCode val;
} static buttonCodes[] = {{"DOWN", Input::Down},
                          {"LEFT", Input::Left},
                          {"RIGHT", Input::Right},
                          {"UP", Input::Up},

#ifdef MARIN
                          {"ZL", Input::ZL},
                          {"ZR", Input::ZR},
#else
                          {"C", Input::ZL},
                          {"Z", Input::ZR},
#endif

                          {"A", Input::A},
                          {"B", Input::B},
                          {"X", Input::X},
                          {"Y", Input::Y},
                          {"L", Input::L},
                          {"R", Input::R},

                          {"SHIFT", Input::Shift},
                          {"CTRL", Input::Ctrl},
                          {"ALT", Input::Alt},

                          {"F5", Input::F5},
                          {"F6", Input::F6},
                          {"F7", Input::F7},
                          {"F8", Input::F8},
                          {"F9", Input::F9},

                          {"MOUSELEFT", Input::MouseLeft},
                          {"MOUSEMIDDLE", Input::MouseMiddle},
                          {"MOUSERIGHT", Input::MouseRight}};

static elementsN(buttonCodes);

void inputBindingInit() {
  VALUE module = rb_define_module("Input");

  _rb_define_module_function(module, "update", inputUpdate);
  _rb_define_module_function(module, "press?", inputPress);
  _rb_define_module_function(module, "trigger?", inputTrigger);
  _rb_define_module_function(module, "repeat?", inputRepeat);
  _rb_define_module_function(module, "pressex?", inputPressEx);
  _rb_define_module_function(module, "triggerex?", inputTriggerEx);
  _rb_define_module_function(module, "repeatex?", inputRepeatEx);
  _rb_define_module_function(module, "dir4", inputDir4);
  _rb_define_module_function(module, "dir8", inputDir8);

  _rb_define_module_function(module, "mouse_x", inputMouseX);
  _rb_define_module_function(module, "mouse_y", inputMouseY);

  _rb_define_module_function(module, "joystick", inputJoystickInfo);
  _rb_define_module_function(module, "rumble", inputRumble);
  _rb_define_module_function(module, "jpressex?", jinputPressEx);
  _rb_define_module_function(module, "jtriggerex?", jinputTriggerEx);
  _rb_define_module_function(module, "jrepeatex?", jinputRepeatEx);

  _rb_define_module_function(module, "text_input", inputGetMode);
  _rb_define_module_function(module, "text_input=", inputSetMode);
  _rb_define_module_function(module, "gets", inputGets);
  _rb_define_module_function(module, "lastKey", inputLastKey);
  _rb_define_module_function(module, "lastJoy", inputLastJoy);
  
  _rb_define_module_function(module, "triggerTreshold", inputGetTriggerTreshold);
  _rb_define_module_function(module, "triggerTreshold=", inputSetTriggerTreshold);

  _rb_define_module_function(module, "clipboard", inputGetClipboard);
  _rb_define_module_function(module, "clipboard=", inputSetClipboard);

  if (rgssVer >= 3) {
    VALUE symHash = rb_hash_new();

    for (size_t i = 0; i < buttonCodesN; ++i) {
      ID sym = rb_intern(buttonCodes[i].str);
      VALUE val = INT2FIX(buttonCodes[i].val);

      /* In RGSS3 all Input::XYZ constants are equal to :XYZ symbols,
       * to be compatible with the previous convention */
      rb_const_set(module, sym, ID2SYM(sym));
      rb_hash_aset(symHash, ID2SYM(sym), val);
    }

    rb_iv_set(module, "buttoncodes", symHash);
    getRbData()->buttoncodeHash = symHash;
  } else {
    for (size_t i = 0; i < buttonCodesN; ++i) {
      ID sym = rb_intern(buttonCodes[i].str);
      VALUE val = INT2FIX(buttonCodes[i].val);

      rb_const_set(module, sym, val);
    }
  }
}
