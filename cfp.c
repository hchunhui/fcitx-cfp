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

#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "internal.h"
#include "fcitx/keys.h"

static void CharFromPhraseFreeStrList(char **list)
{
    char **p;
    for (p = list;*p;p++)
        free(*p);
    free(list);
}

static void
CharFromPhraseModeReset(CFP *cfp)
{
    if (cfp->cfp_mode_lists) {
        int i;
        for (i = 0;i < cfp->cfp_mode_count;i++)
            CharFromPhraseFreeStrList(cfp->cfp_mode_lists[i]);
        free(cfp->cfp_mode_lists);
        cfp->cfp_mode_lists = NULL;
    }
    if (cfp->cfp_mode_selected) {
        free(cfp->cfp_mode_selected);
        cfp->cfp_mode_selected = NULL;
    }
    cfp->cfp_mode_cur = 0;
    cfp->cfp_mode_count = 0;
}

static INPUT_RETURN_VALUE
CharFromPhraseModeGetCandCb(void *arg, FcitxCandidateWord *cand_word)
{
    CFP *cfp = (CFP*)arg;
    int len = strlen(cfp->cfp_mode_selected);
    int len2 = strlen(cand_word->strWord);
    cfp->cfp_mode_selected = realloc(cfp->cfp_mode_selected,
                                           len + len2 + 1);
    memcpy(cfp->cfp_mode_selected + len, cand_word->strWord, len2 + 1);
    FcitxInstanceCommitString(cfp->owner,
                              FcitxInstanceGetCurrentIC(cfp->owner),
                              cfp->cfp_mode_selected);
    return IRV_FLAG_RESET_INPUT | IRV_FLAG_UPDATE_INPUT_WINDOW;
}

static void
CharFromPhraseModeInitCandword(CFP *cfp,
                               FcitxCandidateWord *cand_word)
{
    cand_word->strWord = malloc(UTF8_MAX_LENGTH + 1);
    cand_word->strWord[UTF8_MAX_LENGTH] = '\0';
    cand_word->strExtra = NULL;
    cand_word->callback = CharFromPhraseModeGetCandCb;
    cand_word->wordType = MSG_OTHER;
    cand_word->owner = cfp;
    cand_word->priv = NULL;
}

static boolean
CandwordIsCharFromPhrase(CFP *cfp,
                         FcitxCandidateWord *cand_word)
{
    return (cand_word->callback == CharFromPhraseModeGetCandCb &&
            cand_word->owner == cfp);
}

static void
CharFromPhraseSetClientPreedit(CFP *cfp, const char *str)
{
    FcitxInstance *instance = cfp->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxMessages *client_preedit = FcitxInputStateGetClientPreedit(input);
    FcitxMessagesSetMessageCount(client_preedit, 0);
    FcitxMessagesAddMessageStringsAtLast(client_preedit, MSG_INPUT,
                                         cfp->cfp_mode_selected, str);
}

static void
CharFromPhraseModeUpdateUI(CFP *cfp)
{
    FcitxInstance *instance = cfp->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    FcitxMessages *preedit = FcitxInputStateGetPreedit(input);
    char **cur_list = cfp->cfp_mode_lists[cfp->cfp_mode_cur];
    FcitxCandidateWordSetPage(cand_list, 0);
    FcitxMessagesSetMessageCount(preedit, 0);
    FcitxMessagesAddMessageStringsAtLast(preedit, MSG_INPUT,
                                         cfp->cfp_mode_selected, " (",
                                         cur_list[0], ")");
    CharFromPhraseSetClientPreedit(cfp, *(++cur_list));
    FcitxInputStateSetShowCursor(input, false);
    int i;
    FcitxCandidateWord *cand_word;
    /* use existing cand_word added before if they exist */
    for (i = 0;(cand_word = FcitxCandidateWordGetByTotalIndex(cand_list, i));
         i++) {
        if (CandwordIsCharFromPhrase(cfp, cand_word)) {
            strncpy(cand_word->strWord, *cur_list, UTF8_MAX_LENGTH);
            cur_list++;
            if (!*cur_list) {
                break;
            }
        }
    }
    if (*cur_list) {
        FcitxCandidateWord new_word;
        for (;*cur_list;cur_list++) {
            CharFromPhraseModeInitCandword(cfp, &new_word);
            strncpy(new_word.strWord, *cur_list, UTF8_MAX_LENGTH);
            FcitxCandidateWordAppend(cand_list, &new_word);
        }
    } else {
        for (i++;
             (cand_word = FcitxCandidateWordGetByTotalIndex(cand_list, i));) {
            if (!CandwordIsCharFromPhrase(cfp, cand_word)) {
                i++;
                continue;
            }
            FcitxCandidateWordRemoveByIndex(cand_list, i);
        }
    }
}

static void
CharFromPhraseCheckPage(CFP *cfp)
{
    if (cfp->cfp_cur_word == 0)
        return;
    FcitxInstance *instance = cfp->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    if (FcitxCandidateWordGetCurrentPage(cand_list) == cfp->cfp_cur_page)
        return;
    cfp->cfp_cur_word = 0;
}

static INPUT_RETURN_VALUE
CharFromPhraseStringCommit(CFP *cfp, FcitxKeySym sym)
{
    char *p;
    int index;

    p = strchr(cfp->config.char_from_phrase_str, sym);
    if (!p)
        return IRV_TO_PROCESS;
    index = p - cfp->config.char_from_phrase_str;

    FcitxInstance *instance = cfp->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    if (FcitxCandidateWordGetCurrentWindowSize(cand_list)
        <= cfp->cfp_cur_word) {
        cfp->cfp_cur_word = 0;
    }
    FcitxCandidateWord *cand_word =
        FcitxCandidateWordGetByIndex(cand_list, cfp->cfp_cur_word);
    if (!(cand_word && cand_word->strWord))
        return IRV_TO_PROCESS;

    if (!(*fcitx_utils_get_ascii_end(cand_word->strWord) &&
          *(p = fcitx_utf8_get_nth_char(cand_word->strWord, index))))
        return IRV_DO_NOTHING;

    int len;
    uint32_t chr;
    char *selected;
    char buff[UTF8_MAX_LENGTH + 1];
    FcitxInputContext *cur_ic = FcitxInstanceGetCurrentIC(instance);
    strncpy(buff, p, UTF8_MAX_LENGTH);
    *fcitx_utf8_get_char(buff, &chr) = '\0';
    selected = CFPGetSelected(cfp);
    /* only commit once */
    len = strlen(selected);
    selected = realloc(selected, len + UTF8_MAX_LENGTH + 1);
    strcpy(selected + len, buff);
    FcitxInstanceCommitString(instance, cur_ic, selected);
    free(selected);
    return IRV_FLAG_RESET_INPUT | IRV_FLAG_UPDATE_INPUT_WINDOW;
}

static INPUT_RETURN_VALUE
CharFromPhraseStringSelect(CFP *cfp, FcitxKeySym sym)
{
    FcitxInstance *instance = cfp->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    if (FcitxInputStateGetIsInRemind(input))
        return IRV_TO_PROCESS;
#define SET_CUR_WORD(key, index) case key:      \
    cfp->cfp_cur_word = index; break

    switch (sym) {
        SET_CUR_WORD(FcitxKey_parenright, 9);
        SET_CUR_WORD(FcitxKey_parenleft, 8);
        SET_CUR_WORD(FcitxKey_asterisk, 7);
        SET_CUR_WORD(FcitxKey_ampersand, 6);
        SET_CUR_WORD(FcitxKey_asciicircum, 5);
        SET_CUR_WORD(FcitxKey_percent, 4);
        SET_CUR_WORD(FcitxKey_dollar, 3);
        SET_CUR_WORD(FcitxKey_numbersign, 2);
        SET_CUR_WORD(FcitxKey_at, 1);
        SET_CUR_WORD(FcitxKey_exclam, 0);
    default:
        return IRV_TO_PROCESS;
    }
#undef SET_CUR_WORD

    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    if (FcitxCandidateWordGetCurrentWindowSize(cand_list)
        <= cfp->cfp_cur_word) {
        cfp->cfp_cur_word = 0;
        return IRV_TO_PROCESS;
    }
    cfp->cfp_cur_page = FcitxCandidateWordGetCurrentPage(cand_list);
    return IRV_DO_NOTHING;
}

static INPUT_RETURN_VALUE
CharFromPhraseString(CFP *cfp, FcitxKeySym sym,
                     unsigned int state)
{
    FcitxKeySym keymain = FcitxHotkeyPadToMain(sym);
    INPUT_RETURN_VALUE retval;

    if (!(cfp->config.char_from_phrase_str &&
          *cfp->config.char_from_phrase_str))
        return IRV_TO_PROCESS;
    if (!FcitxHotkeyIsHotKeySimple(keymain, state))
        return IRV_TO_PROCESS;
    if ((retval = CharFromPhraseStringCommit(cfp, keymain)))
        return retval;
    if ((retval = CharFromPhraseStringSelect(cfp, keymain)))
        return retval;
    return IRV_TO_PROCESS;
}

static void
CharFromPhraseSyncPreedit(CFP *cfp,
                          FcitxCandidateWordList *cand_list)
{
    FcitxCandidateWord *cand_word;
    cand_word = FcitxCandidateWordGetCurrentWindow(cand_list);
    if (cand_word && cand_word->strWord) {
        CharFromPhraseSetClientPreedit(cfp, cand_word->strWord);
    } else {
        CharFromPhraseSetClientPreedit(cfp, "");
    }
}

static INPUT_RETURN_VALUE
CharFromPhraseModePre(CFP *cfp, FcitxKeySym sym,
                      unsigned int state)
{
    if (!cfp->cfp_active)
        return IRV_TO_PROCESS;
    FcitxInstance *instance = cfp->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    FcitxGlobalConfig *config = FcitxInstanceGetGlobalConfig(instance);

    int index = FcitxCandidateWordCheckChooseKey(cand_list, sym, state);
    if (index >= 0)
        return FcitxCandidateWordChooseByIndex(cand_list, index);
    if (FcitxHotkeyIsHotKey(sym, state, config->hkPrevPage)) {
        if (FcitxCandidateWordGoPrevPage(cand_list)) {
            CharFromPhraseSyncPreedit(cfp, cand_list);
            return IRV_FLAG_UPDATE_INPUT_WINDOW;
        }
        if (cfp->cfp_mode_cur > 0) {
            cfp->cfp_mode_cur--;
            CharFromPhraseModeUpdateUI(cfp);
            return IRV_FLAG_UPDATE_INPUT_WINDOW;
        }
        return IRV_DO_NOTHING;
    } else if (FcitxHotkeyIsHotKey(sym, state, config->hkNextPage)) {
        if (FcitxCandidateWordGoNextPage(cand_list)) {
            CharFromPhraseSyncPreedit(cfp, cand_list);
            return IRV_FLAG_UPDATE_INPUT_WINDOW;
        }
        if (cfp->cfp_mode_cur < (cfp->cfp_mode_count - 1)) {
            cfp->cfp_mode_cur++;
            CharFromPhraseModeUpdateUI(cfp);
            return IRV_FLAG_UPDATE_INPUT_WINDOW;
        }
        return IRV_DO_NOTHING;
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_HOME)) {
        cfp->cfp_mode_cur = 0;
        CharFromPhraseModeUpdateUI(cfp);
        return IRV_FLAG_UPDATE_INPUT_WINDOW;
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_SPACE)) {
        return FcitxCandidateWordChooseByIndex(cand_list, 0);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_END)) {
        cfp->cfp_mode_cur = cfp->cfp_mode_count - 1;
        CharFromPhraseModeUpdateUI(cfp);
        return IRV_FLAG_UPDATE_INPUT_WINDOW;
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_ENTER)) {
        int len = strlen(cfp->cfp_mode_selected);
        char *cur_full = *cfp->cfp_mode_lists[cfp->cfp_mode_cur];
        int len2 = strlen(cur_full);
        cfp->cfp_mode_selected = realloc(cfp->cfp_mode_selected,
                                               len + len2 + 1);
        memcpy(cfp->cfp_mode_selected + len, cur_full, len2 + 1);
        FcitxInstanceCommitString(cfp->owner,
                                  FcitxInstanceGetCurrentIC(cfp->owner),
                                  cfp->cfp_mode_selected);
        return IRV_FLAG_RESET_INPUT | IRV_FLAG_UPDATE_INPUT_WINDOW;
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_BACKSPACE)) {
        FcitxInstanceCommitString(cfp->owner,
                                  FcitxInstanceGetCurrentIC(cfp->owner),
                                  cfp->cfp_mode_selected);
        return IRV_FLAG_RESET_INPUT | IRV_FLAG_UPDATE_INPUT_WINDOW;
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_ESCAPE)) {
        return IRV_FLAG_RESET_INPUT | IRV_FLAG_UPDATE_INPUT_WINDOW;
    }

    return IRV_DO_NOTHING;
}

/**
 * First element is the original string with ascii characters removed (if any)
 **/
static char**
CharFromPhraseModeListFromWord(const char *word)
{
    if (!(word && *(word = fcitx_utils_get_ascii_end(word))))
        return NULL;
    int n = 0;
    uint32_t chr;
    char *p;
    int len = strlen(word);
    char *buff[len / 2];
    char full[len + 1];
    full[0] = '\0';
    p = fcitx_utf8_get_char(word, &chr);
    if (!*p)
        return NULL;
    do {
        if ((len = p - word) > 1) {
            buff[n] = fcitx_utils_set_str_with_len(NULL, word, len);
            strncat(full, word, len);
            n++;
        }
        if (!*p)
            break;
        word = p;
        p = fcitx_utf8_get_char(word, &chr);
    } while(true);

    /* impossible though */
    if (n <= 0)
        return NULL;
    if (n == 1) {
        free(buff[0]);
        return NULL;
    }

    char **res = malloc(sizeof(char*) * (n + 2));
    res[0] = strdup(full);
    res[n + 1] = NULL;
    for (;n > 0;n--) {
        res[n] = buff[n - 1];
    }
    return res;
}

static boolean
CharFromPhraseModeGetCandLists(CFP *cfp)
{
    FcitxInstance *instance = cfp->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    int size = FcitxCandidateWordGetCurrentWindowSize(cand_list);
    char **lists[size];
    int n = 0;
    int i;
    FcitxCandidateWord *cand_word;
    int cur = 0;
    for (i = 0;i < size;i++) {
        cand_word = FcitxCandidateWordGetByIndex(cand_list, i);
        /* try to turn a candidate word into string list */
        if (cand_word &&
            (lists[n] = CharFromPhraseModeListFromWord(cand_word->strWord))) {
            /* use the same current word index as cfp_string */
            if (i == cfp->cfp_cur_word)
                cur = n;
            n++;
        }
    }
    if (!n)
        return false;

    cfp->cfp_mode_cur = cur;
    cfp->cfp_mode_count = n;
    cfp->cfp_mode_lists = malloc(sizeof(char**) * n);
    memcpy(cfp->cfp_mode_lists, lists, sizeof(char**) * n);
    return true;
}

static INPUT_RETURN_VALUE
CharFromPhraseModePost(CFP *cfp, FcitxKeySym sym,
                       unsigned int state)
{
    if (!FcitxHotkeyIsHotKey(sym, state,
                             cfp->config.char_from_phrase_key))
        return IRV_TO_PROCESS;
    if (!CharFromPhraseModeGetCandLists(cfp))
        return IRV_TO_PROCESS;
    cfp->cfp_mode_selected = CFPGetSelected(cfp);
    cfp->cfp_active = true;
    FcitxInstanceCleanInputWindow(cfp->owner);
    CharFromPhraseModeUpdateUI(cfp);
    return IRV_FLAG_UPDATE_INPUT_WINDOW;
}

boolean
CFPCharFromPhrasePost(CFP *cfp, FcitxKeySym sym,
                                unsigned int state, INPUT_RETURN_VALUE *retval)
{
    CharFromPhraseCheckPage(cfp);
    CharFromPhraseModeReset(cfp);
    if (*retval)
        return false;
    if ((*retval = CharFromPhraseModePost(cfp, sym, state)))
        return true;
    return false;
}

boolean
CFPCharFromPhrasePre(CFP *cfp, FcitxKeySym sym,
                               unsigned int state, INPUT_RETURN_VALUE *retval)
{
    CharFromPhraseCheckPage(cfp);
    if ((*retval = CharFromPhraseString(cfp, sym, state)))
        return true;
    if ((*retval = CharFromPhraseModePre(cfp, sym, state)))
        return true;
    return false;
}

void
CFPCharFromPhraseCandidate(CFP *cfp)
{
    cfp->cfp_cur_word = 0;
    cfp->cfp_cur_page = 0;
    CharFromPhraseModeReset(cfp);
}

void
CFPCharFromPhraseReset(CFP *cfp)
{
    cfp->cfp_cur_word = 0;
    cfp->cfp_cur_page = 0;
    cfp->cfp_active = false;
    CharFromPhraseModeReset(cfp);
}
