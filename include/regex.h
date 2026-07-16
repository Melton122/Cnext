#ifndef CNEXT_REGEX_H
#define CNEXT_REGEX_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int rx_class(const char* pat, size_t* pp, char ch) {
    int neg = 0;
    (*pp)++;
    if (pat[*pp] == '^') { neg = 1; (*pp)++; }
    int hit = 0;
    while (pat[*pp] && pat[*pp] != ']') {
        int a = (unsigned char)pat[*pp];
        if (pat[*pp] == '\\' && pat[*pp + 1]) { (*pp)++; a = (unsigned char)pat[*pp]; }
        if (pat[*pp + 1] && pat[*pp + 1] == '-' && pat[*pp + 2] && pat[*pp + 2] != ']') {
            int b = (unsigned char)pat[*pp + 2];
            if ((ch >= a && ch <= b) || (ch >= b && ch <= a)) hit = 1;
            (*pp) += 2;
        } else if (ch == a) hit = 1;
        (*pp)++;
    }
    if (pat[*pp] == ']') (*pp)++;
    return neg ? !hit : hit;
}

static int rx_char(const char* pat, size_t* pp, char ch) {
    int c = (unsigned char)pat[*pp];
    if (c == '\\' && pat[*pp + 1]) {
        (*pp)++; c = (unsigned char)pat[*pp];
        if (c == 'd') return ch >= '0' && ch <= '9';
        if (c == 'w') return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
        if (c == 's') return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
        return ch == c;
    }
    if (c == '.') return 1;
    if (c == '[') return rx_class(pat, pp, ch);
    return ch == c;
}

static int rx_atom_len(const char* pat, size_t pp) {
    if (pat[pp] == '\\' && pat[pp + 1]) return 2;
    if (pat[pp] == '[') {
        size_t j = pp + 1;
        if (pat[j] == '^') j++;
        while (pat[j] && pat[j] != ']') {
            if (pat[j] == '\\' && pat[j + 1]) j += 2;
            else if (pat[j + 1] == '-' && pat[j + 2]) j += 3;
            else j++;
        }
        if (pat[j] == ']') j++;
        return (int)(j - pp);
    }
    if (pat[pp] && pat[pp] != ')' && pat[pp] != '|' && pat[pp] != '^' && pat[pp] != '$') return 1;
    return 0;
}

static int rx_match(const char* pat, size_t pp, const char* text, size_t tp, size_t tlen);

static int rx_group(const char* pat, size_t pp, const char* text, size_t tp, size_t tlen) {
    pp++;
    while (pat[pp] && pat[pp] != ')') {
        int found_alt = 0;
        size_t end = pp, depth = 1;
        while (pat[end] && depth > 0) {
            if (pat[end] == '(') depth++;
            if (pat[end] == ')') depth--;
            if (depth == 0) break;
            if (pat[end] == '|' && depth == 1) found_alt = 1;
            end++;
        }
        if (found_alt) {
            size_t start = pp;
            while (start < end) {
                size_t bar = start;
                while (bar < end && pat[bar] != '|') bar++;
                char* branch = strndup(pat + start, bar - start);
                if (!branch) return 0;
                int result = rx_match(branch, 0, text, tp, tlen);
                free(branch);
                if (result) return rx_match(pat, end + 1, text, tp, tlen);
                start = bar + 1;
            }
            return 0;
        }
        char* sub = strndup(pat + pp, end - pp);
        if (!sub) return 0;
        int result = rx_match(sub, 0, text, tp, tlen);
        free(sub);
        if (!result) return 0;
        return rx_match(pat, end + 1, text, tp, tlen);
    }
    return rx_match(pat, pp + 1, text, tp, tlen);
}

static int rx_match(const char* pat, size_t pp, const char* text, size_t tp, size_t tlen) {
    while (pat[pp]) {
        if (pat[pp] == ')' || pat[pp] == '|') return 1;
        if (pat[pp] == '^' && pp == 0) { pp++; continue; }
        if (pat[pp] == '$' && pat[pp + 1] == '\0') return tp == tlen;

        if (pat[pp] == '(') {
            int r = rx_group(pat, pp, text, tp, tlen);
            if (r > 0) { int g = r; if (g == 2) g = 1; return g; }
            return 0;
        }

        int alen = rx_atom_len(pat, pp);
        if (alen == 0) { pp++; continue; }

        int quant = 0;
        if (pat[pp + alen] == '*' || pat[pp + alen] == '+' || pat[pp + alen] == '?')
            quant = pat[pp + alen];

        if (quant) {
            int min = (quant == '+') ? 1 : 0;
            int max = (quant == '?') ? 1 : (int)(tlen - tp);
            for (int cnt = max; cnt >= min; cnt--) {
                size_t tmp_tp = tp;
                int ok = 1;
                for (int i = 0; i < cnt; i++) {
                    size_t saved = pp;
                    if (tmp_tp >= tlen || !rx_char(pat, &saved, text[tmp_tp])) { ok = 0; break; }
                    tmp_tp++;
                }
                if (ok) {
                    int r = rx_match(pat, pp + alen + 1, text, tmp_tp, tlen);
                    if (r) return r;
                }
            }
            return 0;
        }

        if (tp >= tlen) return 0;
        size_t saved = pp;
        if (!rx_char(pat, &saved, text[tp])) return 0;
        pp += alen;
        tp++;
    }
    return 1;
}

static int rx_full(const char* pat, const char* text) {
    size_t tlen = strlen(text);
    if (pat[0] == '^') return rx_match(pat, 1, text, 0, tlen);
    for (size_t i = 0; i <= tlen; i++) {
        if (rx_match(pat, 0, text, i, tlen)) return 1;
    }
    return 0;
}

static bool regex_match(CnextString pat, CnextString text) {
    if (!pat.data || !text.data) return false;
    char* p = strndup(pat.data, pat.length);
    char* t = strndup(text.data, text.length);
    int r = rx_full(p, t);
    free(p); free(t);
    return r != 0;
}

static bool regex_search(CnextString pat, CnextString text) {
    return regex_match(pat, text);
}

static CnextString regex_replace(CnextString pat, CnextString repl, CnextString text) {
    if (!pat.data || !text.data) return (CnextString){NULL, 0};
    char* p = strndup(pat.data, pat.length);
    char* t = strndup(text.data, text.length);
    char* r = repl.data ? strndup(repl.data, repl.length) : NULL;
    size_t cap = strlen(t) * 2 + 64;
    char* buf = (char*)malloc(cap);
    if (!buf) { free(p); free(t); free(r); return (CnextString){NULL, 0}; }
    size_t pos = 0, bpos = 0, tlen = strlen(t);
    char* anchored = (char*)malloc(strlen(p) + 3);
    if (!anchored) { free(p); free(t); free(r); free(buf); return (CnextString){NULL, 0}; }
    anchored[0] = '^';
    memcpy(anchored + 1, p, strlen(p));
    anchored[strlen(p) + 1] = '$';
    anchored[strlen(p) + 2] = '\0';
    while (pos < tlen) {
        size_t mlen = 0;
        int found = 0;
        for (size_t end = tlen; end > pos; end--) {
            char sv = t[end]; t[end] = '\0';
            if (rx_full(anchored, t + pos)) { found = 1; mlen = end - pos; t[end] = sv; break; }
            t[end] = sv;
        }
        if (found && mlen > 0) {
            size_t rlen = r ? strlen(r) : 0;
            if (bpos + rlen + 1 > cap) { cap = (bpos + rlen + 1) * 2; char* nb = (char*)realloc(buf, cap); if (!nb) { free(buf); free(anchored); free(p); free(t); free(r); return (CnextString){NULL, 0}; } buf = nb; }
            if (r) { memcpy(buf + bpos, r, rlen); bpos += rlen; }
            pos += mlen;
        } else {
            if (bpos + 2 > cap) { cap = (bpos + 2) * 2; char* nb = (char*)realloc(buf, cap); if (!nb) { free(buf); free(anchored); free(p); free(t); free(r); return (CnextString){NULL, 0}; } buf = nb; }
            buf[bpos++] = t[pos++];
        }
    }
    buf[bpos] = '\0';
    free(anchored); free(p); free(t); free(r);
    _cnext_track(buf);
    return (CnextString){buf, bpos};
}

#endif
