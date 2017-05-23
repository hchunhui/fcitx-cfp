/***************************************************************************
 *   Copyright (C) 2012~2012 by Yichao Yu                                  *
 *   yyc1992@gmail.com                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#ifndef _INTERNAL_H
#define _INTERNAL_H

#include <fcitx/fcitx.h>
#include <fcitx/module.h>
#include <fcitx/instance.h>
#include <fcitx/hook.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utils.h>
#include <fcitx-utils/memory.h>
#include <fcitx/candidate.h>
#include <fcitx-config/xdg.h>

typedef struct {
    FcitxGenericConfig gconfig;
    char *char_from_phrase_str;
    FcitxHotkey char_from_phrase_key[2];
} CFPConfig;

typedef struct {
    CFPConfig config;
    FcitxInstance *owner;

    boolean cfp_active; /* for "char from phrase" */
    int cfp_cur_word;
    int cfp_cur_page;

    char *cfp_mode_selected;
    int cfp_mode_cur;
    int cfp_mode_count;
    char ***cfp_mode_lists;

} CFP;

char *CFPGetSelected(CFP *cfp);

boolean CFPCharFromPhrasePost(CFP *cfp,
                              FcitxKeySym sym, unsigned int state,
                              INPUT_RETURN_VALUE *retval);

boolean CFPCharFromPhrasePre(CFP *cfp,
                             FcitxKeySym sym, unsigned int state,
                             INPUT_RETURN_VALUE *retval);

void CFPCharFromPhraseCandidate(CFP *cfp);
void CFPCharFromPhraseReset(CFP *cfp);

#endif
