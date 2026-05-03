/*
** invoke_string.inl — String method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_STRING.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjString *str = as_string(receiver);

switch (method->string_method_id)
{
case STRING_LEN:
{
    R[base] = val_int(str->length);
    break;
}
case STRING_SUB:
{
    /* str.sub(start, end?) → substring [start, end) */
    int32_t slen = str->length;
    int32_t start = 0, end = slen;
    if (arg_count >= 1 && is_int(args[0])) start = args[0].as.integer;
    if (arg_count >= 2 && is_int(args[1])) end = args[1].as.integer;
    if (start < 0) start += slen;
    if (end < 0) end += slen;
    if (start < 0) start = 0;
    if (end > slen) end = slen;
    if (start >= end)
        R[base] = val_obj((Obj *)new_string(&gc_, "", 0));
    else
        R[base] = val_obj((Obj *)new_string(&gc_, str->chars + start, end - start));
    break;
}
case STRING_FIND:
{
    /* str.find(needle) → index or -1 */
    if (arg_count != 1 || !is_string(args[0])) { runtime_error("find() expects a string argument"); return; }
    ObjString *needle = as_string(args[0]);
    if (needle->length == 0) { R[base] = val_int(0); }
    else {
        const char *found = (const char *)memmem(str->chars, str->length, needle->chars, needle->length);
        R[base] = found ? val_int((int32_t)(found - str->chars)) : val_int(-1);
    }
    break;
}
case STRING_UPPER:
{
    /* str.upper() → new uppercase string */
    char *buf = (char *)malloc(str->length);
    for (int si = 0; si < str->length; si++)
        buf[si] = (str->chars[si] >= 'a' && str->chars[si] <= 'z') ? str->chars[si] - 32 : str->chars[si];
    R[base] = val_obj((Obj *)new_string(&gc_, buf, str->length));
    free(buf);
    break;
}
case STRING_LOWER:
{
    /* str.lower() → new lowercase string */
    char *buf = (char *)malloc(str->length);
    for (int si = 0; si < str->length; si++)
        buf[si] = (str->chars[si] >= 'A' && str->chars[si] <= 'Z') ? str->chars[si] + 32 : str->chars[si];
    R[base] = val_obj((Obj *)new_string(&gc_, buf, str->length));
    free(buf);
    break;
}
case STRING_SPLIT:
{
    /* str.split(sep) → array of strings */
    if (arg_count != 1 || !is_string(args[0])) { runtime_error("split() expects a string argument"); return; }
    ObjString *sep = as_string(args[0]);
    ObjArray *result = new_array(&gc_);
    if (sep->length == 0) {
        /* Split into individual chars */
        for (int si = 0; si < str->length; si++)
            array_push(&gc_, result, val_obj((Obj *)new_string(&gc_, &str->chars[si], 1)));
    } else {
        const char *start_ptr = str->chars;
        const char *end_ptr = str->chars + str->length;
        while (start_ptr <= end_ptr) {
            const char *found = (const char *)memmem(start_ptr, end_ptr - start_ptr, sep->chars, sep->length);
            if (!found) {
                array_push(&gc_, result, val_obj((Obj *)new_string(&gc_, start_ptr, (int)(end_ptr - start_ptr))));
                break;
            }
            array_push(&gc_, result, val_obj((Obj *)new_string(&gc_, start_ptr, (int)(found - start_ptr))));
            start_ptr = found + sep->length;
        }
    }
    R[base] = val_obj((Obj *)result);
    break;
}
case STRING_TRIM:
{
    /* str.trim() → strip leading/trailing whitespace */
    int start = 0, end = str->length;
    while (start < end && (str->chars[start] == ' ' || str->chars[start] == '\t' ||
                           str->chars[start] == '\n' || str->chars[start] == '\r'))
        start++;
    while (end > start && (str->chars[end-1] == ' ' || str->chars[end-1] == '\t' ||
                           str->chars[end-1] == '\n' || str->chars[end-1] == '\r'))
        end--;
    R[base] = val_obj((Obj *)new_string(&gc_, str->chars + start, end - start));
    break;
}
case STRING_REPLACE:
{
    /* str.replace(old, new) → new string with all occurrences replaced */
    if (arg_count != 2 || !is_string(args[0]) || !is_string(args[1])) {
        runtime_error("replace() expects (string, string)"); return;
    }
    ObjString *old_s = as_string(args[0]);
    ObjString *new_s = as_string(args[1]);
    if (old_s->length == 0) { R[base] = receiver; }
    else {
        /* Count occurrences first */
        int occurrences = 0;
        const char *sp = str->chars;
        const char *ep = str->chars + str->length;
        while (sp < ep) {
            const char *f = (const char *)memmem(sp, ep - sp, old_s->chars, old_s->length);
            if (!f) break;
            occurrences++;
            sp = f + old_s->length;
        }
        if (occurrences == 0) { R[base] = receiver; }
        else {
            int new_len = str->length + occurrences * (new_s->length - old_s->length);
            char *buf = (char *)malloc(new_len);
            char *wp = buf;
            sp = str->chars;
            while (sp < ep) {
                const char *f = (const char *)memmem(sp, ep - sp, old_s->chars, old_s->length);
                if (!f) { memcpy(wp, sp, ep - sp); wp += (ep - sp); break; }
                memcpy(wp, sp, f - sp); wp += (f - sp);
                memcpy(wp, new_s->chars, new_s->length); wp += new_s->length;
                sp = f + old_s->length;
            }
            R[base] = val_obj((Obj *)new_string(&gc_, buf, new_len));
            free(buf);
        }
    }
    break;
}
case STRING_STARTS_WITH:
{
    if (arg_count != 1 || !is_string(args[0])) { runtime_error("starts_with() expects a string"); return; }
    ObjString *prefix = as_string(args[0]);
    bool match = (prefix->length <= str->length) &&
                 (memcmp(str->chars, prefix->chars, prefix->length) == 0);
    R[base] = val_bool(match);
    break;
}
case STRING_ENDS_WITH:
{
    if (arg_count != 1 || !is_string(args[0])) { runtime_error("ends_with() expects a string"); return; }
    ObjString *suffix = as_string(args[0]);
    bool match = (suffix->length <= str->length) &&
                 (memcmp(str->chars + str->length - suffix->length, suffix->chars, suffix->length) == 0);
    R[base] = val_bool(match);
    break;
}
case STRING_CHAR_AT:
{
    /* str.char_at(idx) → single-char string */
    if (arg_count != 1 || !is_int(args[0])) { runtime_error("char_at() expects an integer"); return; }
    int32_t idx = args[0].as.integer;
    if (idx < 0 || idx >= str->length) { R[base] = val_nil(); }
    else { R[base] = val_obj((Obj *)new_string(&gc_, &str->chars[idx], 1)); }
    break;
}
case STRING_BYTE_AT:
{
    /* str.byte_at(idx) → integer byte value */
    if (arg_count != 1 || !is_int(args[0])) { runtime_error("byte_at() expects an integer"); return; }
    int32_t idx = args[0].as.integer;
    if (idx < 0 || idx >= str->length) { R[base] = val_int(0); }
    else { R[base] = val_int((uint8_t)str->chars[idx]); }
    break;
}
default:
{
    runtime_error("string has no method '%s'", mname);
    return;
}
}
