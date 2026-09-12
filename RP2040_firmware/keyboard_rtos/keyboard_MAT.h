//////////////////////////////----for keyboard----/////////////////////////////////
#define KC_A        (uint16_t)'a'
#define KC_B        (uint16_t)'b'
#define KC_C        (uint16_t)'c'
#define KC_D        (uint16_t)'d'
#define KC_E        (uint16_t)'e'
#define KC_F        (uint16_t)'f'
#define KC_G        (uint16_t)'g'
#define KC_H        (uint16_t)'h'
#define KC_I        (uint16_t)'i'
#define KC_J        (uint16_t)'j'
#define KC_K        (uint16_t)'k'
#define KC_L        (uint16_t)'l'
#define KC_M        (uint16_t)'m'
#define KC_N        (uint16_t)'n'
#define KC_O        (uint16_t)'o'
#define KC_P        (uint16_t)'p'
#define KC_Q        (uint16_t)'q'
#define KC_R        (uint16_t)'r'
#define KC_S        (uint16_t)'s'
#define KC_T        (uint16_t)'t'
#define KC_U        (uint16_t)'u'
#define KC_V        (uint16_t)'v'
#define KC_W        (uint16_t)'w'
#define KC_X        (uint16_t)'x'
#define KC_Y        (uint16_t)'y'
#define KC_Z        (uint16_t)'z'
#define KC_SPC      (uint16_t)' '
#define KC_ENT      (uint16_t)KEY_RETURN
#define KC_BSPC     (uint16_t)KEY_BACKSPACE
#define KC_TAB      (uint16_t)KEY_TAB
#define KC_CAPS     (uint16_t)KEY_CAPS_LOCK
#define KC_LCTRL    (uint16_t)KEY_LEFT_CTRL
#define KC_LSHFT    (uint16_t)KEY_LEFT_SHIFT
#define KC_LALT     (uint16_t)KEY_LEFT_ALT
#define KC_LCLICK   (uint16_t)0xE001
#define KC_RCLICK   (uint16_t)0xE002
#define KC_LGUI     (uint16_t)KEY_LEFT_GUI
#define KC_NO       (uint16_t)0xFFFF
#define KC_1        (uint16_t)'1'
#define KC_2        (uint16_t)'2'
#define KC_3        (uint16_t)'3'
#define KC_4        (uint16_t)'4'
#define KC_5        (uint16_t)'5'
#define KC_6        (uint16_t)'6'
#define KC_7        (uint16_t)'7'
#define KC_8        (uint16_t)'8'
#define KC_9        (uint16_t)'9'
#define KC_0        (uint16_t)'0'

#define KC_SLSH     (uint16_t)'/'
#define KC_COLN     (uint16_t)':'
#define KC_SCLN     (uint16_t)';'
#define KC_QUOT     (uint16_t)'\''
#define KC_DQUO     (uint16_t)'\"'
#define KC_QUES     (uint16_t)'?'
#define KC_EXLM     (uint16_t)'!'
#define KC_COMM     (uint16_t)','
#define KC_DOT      (uint16_t)'.'
#define KC_ASTR     (uint16_t)'*'
#define KC_HASH     (uint16_t)'#'

const uint16_t key_Mat[7][7] PROGMEM = {
  { KC_LCLICK,  KC_W,     KC_G,     KC_S,     KC_L,     KC_H,     KC_NO  },
  { KC_NO,      KC_Q,     KC_R,     KC_E,     KC_O,     KC_U,     KC_NO  },
  { KC_NO,      KC_NO,    KC_F,     KC_CAPS,  KC_K,     KC_J,     KC_NO  },
  { KC_NO,      KC_SPC,   KC_C,     KC_Z,     KC_M,     KC_N,     KC_NO  },
  { KC_LGUI,    KC_LCTRL, KC_T,     KC_D,     KC_I,     KC_Y,     KC_NO  },
  { KC_RCLICK,  KC_LALT,  KC_V,     KC_X,     KC_LSHFT, KC_B,     KC_NO  },
  { KC_NO,      KC_A,     KC_NO,    KC_P,     KC_BSPC,  KC_ENT,   KC_NO  }
};

//number, shift, ctrl, fn, tab,... matrix
const uint16_t key_Mat_num[7][7] PROGMEM = {
  { KC_LCLICK,  KC_1,       KC_SLSH,    KC_4,     KC_L,     KC_COLN,  KC_NO  },
  { KC_NO,      KC_HASH,    KC_3,       KC_2,     KC_O,     KC_U,     KC_NO  },
  { KC_NO,      KC_NO,      KC_6,       KC_CAPS,  KC_K,     KC_SCLN,  KC_NO  },
  { KC_NO,      KC_SPC,     KC_9,       KC_7,     KC_DOT,   KC_COMM,  KC_NO  },
  { KC_LGUI,    KC_LCTRL,   KC_T,       KC_5,     KC_I,     KC_Y,     KC_NO  },
  { KC_RCLICK,  KC_LALT,    KC_QUES,    KC_8,     KC_LSHFT, KC_EXLM,  KC_NO  },
  { KC_NO,      KC_ASTR,    KC_NO,      KC_P,     KC_BSPC,  KC_ENT,   KC_NO  }
};