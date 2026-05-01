/*
** invoke_array.inl — Array method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_ARRAY.
**
** Available variables:
**   base      — register index of receiver (R[base] = array)
**   arg_count — number of arguments
**   receiver  — Value of the array
**   mname     — method name (const char*)
**   mlen      — method name length
**   args      — pointer to first argument (R[base+1])
**   R         — register file
**   K         — constant pool
*/

ObjArray *arr = as_array(receiver);

/* Dispatch by method name (sorted by likely frequency) */
if (mlen == 4 && memcmp(mname, "push", 4) == 0)
{
    /* arr.push(val) → append, returns new length */
    if (arg_count < 1)
    {
        runtime_error("push() expects at least 1 argument");
        return;
    }
    for (int ai = 0; ai < arg_count; ai++)
        array_push(&gc_, arr, args[ai]);
    R[base] = val_int(arr_count(arr));
}
else if (mlen == 3 && memcmp(mname, "pop", 3) == 0)
{
    /* arr.pop() → remove+return last element */
    if (arr_count(arr) == 0)
    {
        runtime_error("pop() on empty array");
        return;
    }
    R[base] = *--arr->end;
}
else if (mlen == 3 && memcmp(mname, "len", 3) == 0)
{
    /* arr.len() → length */
    R[base] = val_int(arr_count(arr));
}
else if (mlen == 6 && memcmp(mname, "remove", 6) == 0)
{
    /* arr.remove(idx) → remove at index, return removed value */
    if (arg_count != 1 || !is_int(args[0]))
    {
        runtime_error("remove() expects 1 integer argument");
        return;
    }
    int32_t idx = args[0].as.integer;
    int32_t count = arr_count(arr);
    if (idx < 0 || idx >= count)
    {
        runtime_error("remove() index out of bounds");
        return;
    }
    Value removed = arr->data[idx];
    memmove(&arr->data[idx], &arr->data[idx + 1], (size_t)(count - idx - 1) * sizeof(Value));
    arr->end--;
    R[base] = removed;
}
else if (mlen == 6 && memcmp(mname, "insert", 6) == 0)
{
    /* arr.insert(idx, val) → insert at position */
    if (arg_count != 2 || !is_int(args[0]))
    {
        runtime_error("insert() expects (int, value)");
        return;
    }
    int32_t idx = args[0].as.integer;
    int32_t count = arr_count(arr);
    if (idx < 0 || idx > count)
    {
        runtime_error("insert() index out of bounds");
        return;
    }
    array_push(&gc_, arr, val_nil()); /* ensure capacity, end++ */
    /* shift right */
    memmove(&arr->data[idx + 1], &arr->data[idx], (size_t)(count - idx) * sizeof(Value));
    arr->data[idx] = args[1];
    R[base] = val_int(arr_count(arr));
}
else if (mlen == 5 && memcmp(mname, "slice", 5) == 0)
{
    /* arr.slice(start, end?) → new array [start, end) */
    int32_t count = arr_count(arr);
    int32_t start = 0, end_idx = count;
    if (arg_count >= 1 && is_int(args[0]))
        start = args[0].as.integer;
    if (arg_count >= 2 && is_int(args[1]))
        end_idx = args[1].as.integer;
    if (start < 0)
        start += count;
    if (end_idx < 0)
        end_idx += count;
    if (start < 0)
        start = 0;
    if (end_idx > count)
        end_idx = count;
    ObjArray *result = new_array(&gc_);
    if (start < end_idx)
    {
        int32_t new_count = end_idx - start;
        array_reserve(&gc_, result, new_count);
        memcpy(result->data, &arr->data[start], (size_t)new_count * sizeof(Value));
        result->end = result->data + new_count;
    }
    R[base] = val_obj((Obj *)result);
}
else if (mlen == 7 && memcmp(mname, "reverse", 7) == 0)
{
    /* arr.reverse() → in-place reverse, returns arr */
    array_reverse(arr);
    R[base] = receiver;
}
else if (mlen == 5 && memcmp(mname, "clear", 5) == 0)
{
    /* arr.clear() → empty the array */
    array_clear(arr);
    R[base] = val_nil();
}
else if (mlen == 8 && memcmp(mname, "contains", 8) == 0)
{
    /* arr.contains(val) → bool */
    if (arg_count != 1)
    {
        runtime_error("contains() expects 1 argument");
        return;
    }
    R[base] = val_bool(array_contains(arr, args[0]));
}
else if (mlen == 4 && memcmp(mname, "join", 4) == 0)
{
    /* arr.join(sep?) → string */
    const char *sep = "";
    int sep_len = 0;
    if (arg_count >= 1 && is_string(args[0]))
    {
        sep = as_cstring(args[0]);
        sep_len = as_string(args[0])->length;
    }
    int32_t count = arr_count(arr);
    char num_buf[64];
    /* First pass: compute length */
    int total_len = 0;
    for (int32_t ji = 0; ji < count; ji++)
    {
        if (ji > 0)
            total_len += sep_len;
        Value v = arr->data[ji];
        if (is_string(v))
            total_len += as_string(v)->length;
        else if (is_int(v))
            total_len += int_to_cstr(v.as.integer, num_buf);
        else if (is_float(v))
            total_len += snprintf(num_buf, sizeof(num_buf), "%g", v.as.number);
        else if (is_nil(v))
            total_len += 3;
        else if (is_bool(v))
            total_len += v.as.boolean ? 4 : 5;
        else
            total_len += 3;
    }
    /* Second pass: build string */
    char *buf = (char *)malloc(total_len + 1);
    char *p = buf;
    for (int32_t ji = 0; ji < count; ji++)
    {
        if (ji > 0)
        {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
        Value v = arr->data[ji];
        if (is_string(v))
        {
            memcpy(p, as_cstring(v), as_string(v)->length);
            p += as_string(v)->length;
        }
        else if (is_int(v))
        {
            int n = int_to_cstr(v.as.integer, num_buf);
            memcpy(p, num_buf, n);
            p += n;
        }
        else if (is_float(v))
        {
            int n = snprintf(num_buf, sizeof(num_buf), "%g", v.as.number);
            memcpy(p, num_buf, n);
            p += n;
        }
        else if (is_nil(v))
        {
            memcpy(p, "nil", 3);
            p += 3;
        }
        else if (is_bool(v))
        {
            const char *s = v.as.boolean ? "true" : "false";
            int n = v.as.boolean ? 4 : 5;
            memcpy(p, s, n);
            p += n;
        }
        else
        {
            memcpy(p, "???", 3);
            p += 3;
        }
    }
    R[base] = val_obj((Obj *)new_string(&gc_, buf, total_len));
    free(buf);
}
else if (mlen == 4 && memcmp(mname, "sort", 4) == 0)
{
    /* arr.sort() or arr.sort("desc") → in-place sort using qsort */
    if (arg_count > 1)
    {
        runtime_error("sort() expects 0 or 1 argument");
        return;
    }
    bool descending = false;
    if (arg_count == 1)
    {
        if (!is_string(args[0]))
        {
            runtime_error("sort() argument must be a string (\"asc\" or \"desc\")");
            return;
        }
        ObjString *order = as_string(args[0]);
        if (order->length == 4 && memcmp(order->chars, "desc", 4) == 0)
            descending = true;
    }
    int32_t count = arr_count(arr);
    if (count > 1)
    {
        /* qsort with static comparator — store direction in a thread-local (ok for single-threaded VM) */
        static bool s_desc;
        s_desc = descending;
        qsort(arr->data, (size_t)count, sizeof(Value), [](const void *pa, const void *pb) -> int
              {
            Value a = *(const Value *)pa, b = *(const Value *)pb;
            int cmp = 0;
            double da = 0, db = 0;
            bool a_num = is_int(a) || is_float(a);
            bool b_num = is_int(b) || is_float(b);
            if (a_num && b_num) {
                da = is_int(a) ? (double)a.as.integer : a.as.number;
                db = is_int(b) ? (double)b.as.integer : b.as.number;
                cmp = (da > db) - (da < db);
            } else if (is_string(a) && is_string(b)) {
                int minlen = as_string(a)->length < as_string(b)->length ? as_string(a)->length : as_string(b)->length;
                cmp = memcmp(as_cstring(a), as_cstring(b), minlen);
                if (cmp == 0) cmp = as_string(a)->length - as_string(b)->length;
            } else {
                /* numbers before strings before others */
                cmp = (int)a.type - (int)b.type;
            }
            return s_desc ? -cmp : cmp; });
    }
    R[base] = receiver;
}
else if (mlen == 8 && memcmp(mname, "index_of", 8) == 0)
{
    /* arr.index_of(val) → index or -1 */
    if (arg_count != 1)
    {
        runtime_error("index_of() expects 1 argument");
        return;
    }
    R[base] = val_int(array_find(arr, args[0]));
}
else
{
    runtime_error("array has no method '%s'", mname);
    return;
}
