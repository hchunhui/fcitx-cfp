/***************************************************************************
 *   Copyright (C) 2011~2012 by CSSlayer                                   *
 *   wengxt@gmail.com                                                      *
 *   Copyright (C) 2012~2013 by Yichao Yu                                  *
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

#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "internal.h"

DECLARE_ADDFUNCTIONS(CFP)

CONFIG_BINDING_BEGIN(CFPConfig)
CONFIG_BINDING_REGISTER("CFP", "InputCharFromPhraseString",
                        char_from_phrase_str);
CONFIG_BINDING_REGISTER("CFP", "InputCharFromPhraseKey",
                        char_from_phrase_key);
CONFIG_BINDING_END()

CONFIG_DEFINE_LOAD_AND_SAVE(CFP, CFPConfig,
                            "fcitx-cfp")

static int
check_im_type(CFP *cfp)
{
    FcitxIM *im = FcitxInstanceGetCurrentIM(cfp->owner);
    if (!im)
        return 0;
    if(strstr(im->uniqueName, "rime"))
        return 1;
    return 0;
}

static boolean
CFPPreInput(void *arg, FcitxKeySym sym, unsigned int state,
                      INPUT_RETURN_VALUE *retval)
{
    CFP *cfp = (CFP*)arg;
    if (!check_im_type(cfp))
        return false;
    if (CFPCharFromPhrasePre(cfp, sym, state, retval))
        return true;
    return false;
}

static boolean
CFPPostInput(void *arg, FcitxKeySym sym, unsigned int state,
                       INPUT_RETURN_VALUE *retval)
{
    CFP *cfp = (CFP*)arg;
    if (!check_im_type(cfp))
        return false;
    if (CFPCharFromPhrasePost(cfp, sym, state, retval))
        return true;
    return false;
}

static void
CFPAddCandidateWord(void *arg)
{
    CFP *cfp = (CFP*)arg;
    /* reset cfp */
    CFPCharFromPhraseCandidate(cfp);
}

static void
CFPReloadConfig(void *arg)
{
    CFP *cfp = (CFP*)arg;
    CFPLoadConfig(&cfp->config);
}

char*
CFPGetSelected(CFP *cfp)
{
    FcitxInputState *input;
    char *string;
    input = FcitxInstanceGetInputState(cfp->owner);
    string = FcitxUIMessagesToCString(FcitxInputStateGetPreedit(input));
    /**
     * Haven't found a way to handle the case when the word before the current
     * one is not handled by im-engine (e.g. a invalid pinyin in sunpinyin).
     * (a possible solution is to deal with different im-engine separately).
     * Not very important though....
     **/
    *fcitx_utils_get_ascii_part(string) = '\0';
    return string;
}

static void
CFPResetHook(void *arg)
{
    CFP *cfp = (CFP*)arg;
    CFPCharFromPhraseReset(cfp);
}

static void*
CFPCreate(FcitxInstance *instance)
{
    CFP *cfp = fcitx_utils_new(CFP);
    cfp->owner = instance;

    if (!CFPLoadConfig(&cfp->config)) {
        free(cfp);
        return NULL;
    }

    FcitxIMEventHook event_hook = {
        .arg = cfp,
        .func = CFPAddCandidateWord,
    };
    FcitxInstanceRegisterUpdateCandidateWordHook(instance, event_hook);
    event_hook.func = CFPResetHook;
    FcitxInstanceRegisterResetInputHook(instance, event_hook);

    FcitxKeyFilterHook key_hook = {
        .arg = cfp,
        .func = CFPPostInput
    };
    FcitxInstanceRegisterPostInputFilter(cfp->owner, key_hook);
    key_hook.func = CFPPreInput;
    FcitxInstanceRegisterPreInputFilter(cfp->owner, key_hook);

    FcitxCFPAddFunctions(instance);
    return cfp;
}

static void
CFPDestroy(void *arg)
{
}

FCITX_DEFINE_PLUGIN(fcitx_cfp, module, FcitxModule) = {
    .Create = CFPCreate,
    .Destroy = CFPDestroy,
    .SetFD = NULL,
    .ProcessEvent = NULL,
    .ReloadConfig = CFPReloadConfig
};

#include "fcitx-cfp-addfunctions.h"
