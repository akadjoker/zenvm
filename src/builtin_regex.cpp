#ifdef ZEN_ENABLE_REGEX

/* =========================================================
** builtin_regex.cpp — "re" module for Zen (PCRE2)
**
** Full regex support via PCRE2 (system library).
**
** Usage:
**   import re;
**   var m = re.match("(\\d+)-(\\w+)", "123-abc");
**   print(m);          // "123-abc"
**   print(re.group(m, 1)); // "123"
**   print(re.group(m, 2)); // "abc"
**
**   var all = re.find_all("\\d+", "foo 12 bar 34 baz 56");
**   // ["12", "34", "56"]
**
**   var s = re.replace("\\d+", "NUM", "foo 12 bar 34");
**   // "foo NUM bar NUM"
**
**   var parts = re.split(",\\s*", "a, b, c, d");
**   // ["a", "b", "c", "d"]
** ========================================================= */

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#include "module.h"
#include "vm.h"
#include <cstring>
#include <cstdlib>

namespace zen
{

    /* =========================================================
    ** Helpers
    ** ========================================================= */

    /* Compile pattern, return code or nullptr (sets error on vm) */
    static pcre2_code *compile_pattern(VM *vm, const char *pattern, int len)
    {
        int errornumber;
        PCRE2_SIZE erroroffset;
        pcre2_code *re = pcre2_compile(
            (PCRE2_SPTR)pattern,
            (PCRE2_SIZE)len,
            PCRE2_UTF,
            &errornumber,
            &erroroffset,
            nullptr);
        if (!re)
        {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
            vm->runtime_error("re: compile error at offset %d: %s", (int)erroroffset, buffer);
        }
        return re;
    }

    /* =========================================================
    ** match(pattern, subject) → string or nil
    ** Full match of subject against pattern.
    ** ========================================================= */
    static int nat_re_match(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("re.match() expects (pattern, subject).");
            return -1;
        }

        ObjString *pat = as_string(args[0]);
        ObjString *subj = as_string(args[1]);

        pcre2_code *re = compile_pattern(vm, pat->chars, pat->length);
        if (!re)
            return -1;

        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, nullptr);
        int rc = pcre2_match(re, (PCRE2_SPTR)subj->chars, (PCRE2_SIZE)subj->length,
                             0, 0, match_data, nullptr);

        if (rc < 0)
        {
            pcre2_match_data_free(match_data);
            pcre2_code_free(re);
            args[0] = val_nil();
            return 1;
        }

        /* Return the full match as string */
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        int start = (int)ovector[0];
        int end = (int)ovector[1];
        args[0] = val_obj((Obj *)vm->make_string(subj->chars + start, end - start));

        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
        return 1;
    }

    /* =========================================================
    ** search(pattern, subject) → array [full, group1, ...] or nil
    ** First match with captures.
    ** ========================================================= */
    static int nat_re_search(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("re.search() expects (pattern, subject).");
            return -1;
        }

        ObjString *pat = as_string(args[0]);
        ObjString *subj = as_string(args[1]);

        pcre2_code *re = compile_pattern(vm, pat->chars, pat->length);
        if (!re)
            return -1;

        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, nullptr);
        int rc = pcre2_match(re, (PCRE2_SPTR)subj->chars, (PCRE2_SIZE)subj->length,
                             0, 0, match_data, nullptr);

        if (rc < 0)
        {
            pcre2_match_data_free(match_data);
            pcre2_code_free(re);
            args[0] = val_nil();
            return 1;
        }

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        int pairs = rc; /* number of pairs */

        /* Create result array */
        ObjArray *arr = new_array(&vm->get_gc());
        for (int i = 0; i < pairs; i++)
        {
            int s = (int)ovector[2 * i];
            int e = (int)ovector[2 * i + 1];
            if (s >= 0 && e >= s)
            {
                Value str = val_obj((Obj *)vm->make_string(subj->chars + s, e - s));
                array_push(&vm->get_gc(), arr, str);
            }
            else
            {
                array_push(&vm->get_gc(), arr, val_nil());
            }
        }

        args[0] = val_obj((Obj *)arr);
        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
        return 1;
    }

    /* =========================================================
    ** find_all(pattern, subject) → array of strings
    ** All non-overlapping matches (full match only).
    ** ========================================================= */
    static int nat_re_find_all(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("re.find_all() expects (pattern, subject).");
            return -1;
        }

        ObjString *pat = as_string(args[0]);
        ObjString *subj = as_string(args[1]);

        pcre2_code *re = compile_pattern(vm, pat->chars, pat->length);
        if (!re)
            return -1;

        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, nullptr);
        ObjArray *result = new_array(&vm->get_gc());

        PCRE2_SIZE offset = 0;
        while (offset <= (PCRE2_SIZE)subj->length)
        {
            int rc = pcre2_match(re, (PCRE2_SPTR)subj->chars, (PCRE2_SIZE)subj->length,
                                 offset, 0, match_data, nullptr);
            if (rc < 0)
                break;

            PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
            int s = (int)ovector[0];
            int e = (int)ovector[1];

            Value str = val_obj((Obj *)vm->make_string(subj->chars + s, e - s));
            array_push(&vm->get_gc(), result, str);

            /* Advance past this match (avoid infinite loop on zero-length) */
            offset = (PCRE2_SIZE)e;
            if (e == s)
                offset++;
        }

        args[0] = val_obj((Obj *)result);
        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
        return 1;
    }

    /* =========================================================
    ** replace(pattern, replacement, subject, max?) → string
    ** Replace all (or up to max) occurrences.
    ** ========================================================= */
    static int nat_re_replace(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3 || !is_string(args[0]) || !is_string(args[1]) || !is_string(args[2]))
        {
            vm->runtime_error("re.replace() expects (pattern, replacement, subject).");
            return -1;
        }

        ObjString *pat = as_string(args[0]);
        ObjString *repl = as_string(args[1]);
        ObjString *subj = as_string(args[2]);
        int max_replacements = (nargs >= 4 && is_int(args[3])) ? (int)args[3].as.integer : -1;

        pcre2_code *re = compile_pattern(vm, pat->chars, pat->length);
        if (!re)
            return -1;

        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, nullptr);

        /* Build result with manual iteration */
        int capacity = subj->length + 64;
        char *buf = (char *)malloc((size_t)capacity);
        int buf_len = 0;
        PCRE2_SIZE offset = 0;
        int count = 0;

        while (offset <= (PCRE2_SIZE)subj->length)
        {
            if (max_replacements >= 0 && count >= max_replacements)
                break;

            int rc = pcre2_match(re, (PCRE2_SPTR)subj->chars, (PCRE2_SIZE)subj->length,
                                 offset, 0, match_data, nullptr);
            if (rc < 0)
                break;

            PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
            int s = (int)ovector[0];
            int e = (int)ovector[1];

            /* Copy text before match */
            int pre_len = s - (int)offset;
            int needed = buf_len + pre_len + repl->length + 1;
            if (needed > capacity)
            {
                capacity = needed * 2;
                buf = (char *)realloc(buf, (size_t)capacity);
            }
            memcpy(buf + buf_len, subj->chars + offset, (size_t)pre_len);
            buf_len += pre_len;

            /* Copy replacement */
            memcpy(buf + buf_len, repl->chars, (size_t)repl->length);
            buf_len += repl->length;

            offset = (PCRE2_SIZE)e;
            if (e == s)
                offset++;
            count++;
        }

        /* Copy remainder */
        int rem = subj->length - (int)offset;
        if (rem > 0)
        {
            if (buf_len + rem + 1 > capacity)
            {
                capacity = buf_len + rem + 1;
                buf = (char *)realloc(buf, (size_t)capacity);
            }
            memcpy(buf + buf_len, subj->chars + offset, (size_t)rem);
            buf_len += rem;
        }

        args[0] = val_obj((Obj *)vm->make_string(buf, buf_len));
        free(buf);
        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
        return 1;
    }

    /* =========================================================
    ** split(pattern, subject, max?) → array of strings
    ** Split subject by pattern matches.
    ** ========================================================= */
    static int nat_re_split(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("re.split() expects (pattern, subject).");
            return -1;
        }

        ObjString *pat = as_string(args[0]);
        ObjString *subj = as_string(args[1]);
        int max_splits = (nargs >= 3 && is_int(args[2])) ? (int)args[2].as.integer : -1;

        pcre2_code *re = compile_pattern(vm, pat->chars, pat->length);
        if (!re)
            return -1;

        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, nullptr);
        ObjArray *result = new_array(&vm->get_gc());

        PCRE2_SIZE offset = 0;
        int count = 0;

        while (offset <= (PCRE2_SIZE)subj->length)
        {
            if (max_splits >= 0 && count >= max_splits)
                break;

            int rc = pcre2_match(re, (PCRE2_SPTR)subj->chars, (PCRE2_SIZE)subj->length,
                                 offset, 0, match_data, nullptr);
            if (rc < 0)
                break;

            PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
            int s = (int)ovector[0];
            int e = (int)ovector[1];

            /* Add segment before match */
            int seg_len = s - (int)offset;
            Value seg = val_obj((Obj *)vm->make_string(subj->chars + (int)offset, seg_len));
            array_push(&vm->get_gc(), result, seg);

            offset = (PCRE2_SIZE)e;
            if (e == s)
                offset++;
            count++;
        }

        /* Add final segment */
        int rem = subj->length - (int)offset;
        Value last = val_obj((Obj *)vm->make_string(subj->chars + (int)offset, rem >= 0 ? rem : 0));
        array_push(&vm->get_gc(), result, last);

        args[0] = val_obj((Obj *)result);
        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
        return 1;
    }

    /* =========================================================
    ** test(pattern, subject) → bool
    ** Quick check if pattern matches anywhere in subject.
    ** ========================================================= */
    static int nat_re_test(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("re.test() expects (pattern, subject).");
            return -1;
        }

        ObjString *pat = as_string(args[0]);
        ObjString *subj = as_string(args[1]);

        pcre2_code *re = compile_pattern(vm, pat->chars, pat->length);
        if (!re)
            return -1;

        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, nullptr);
        int rc = pcre2_match(re, (PCRE2_SPTR)subj->chars, (PCRE2_SIZE)subj->length,
                             0, 0, match_data, nullptr);

        args[0] = val_bool(rc >= 0);
        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
        return 1;
    }

    /* =========================================================
    ** Registration
    ** ========================================================= */

    static const NativeReg re_functions[] = {
        {"match", nat_re_match, 2},
        {"search", nat_re_search, 2},
        {"find_all", nat_re_find_all, 2},
        {"replace", nat_re_replace, -1},
        {"split", nat_re_split, -1},
        {"test", nat_re_test, 2},
    };

    const NativeLib zen_lib_re = {
        "re",
        re_functions,
        6,
        nullptr,
        0,
    };

} /* namespace zen */

#endif /* ZEN_ENABLE_REGEX */
