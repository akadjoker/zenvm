/*
** invoke_buffer.inl — Buffer method dispatch for OP_INVOKE.
** Minimal API — raw speed via buf[i] / buf[i]=x is the primary interface.
*/

ObjBuffer *buf = as_buffer(receiver);

if (mlen == 3 && memcmp(mname, "len", 3) == 0)
{
    R[base] = val_int(buf->count);
}
else if (mlen == 4 && memcmp(mname, "fill", 4) == 0)
{
    if (arg_count != 1)
    {
        runtime_error("fill() expects 1 argument");
        return;
    }
    double v = 0;
    if (is_int(args[0]))
        v = (double)args[0].as.integer;
    else if (is_float(args[0]))
        v = args[0].as.number;
    else
    {
        runtime_error("fill() expects a number");
        return;
    }
    buffer_fill(buf, v);
    R[base] = receiver;
}
else if (mlen == 8 && memcmp(mname, "byte_len", 8) == 0)
{
    R[base] = val_int(buf->count * buffer_elem_size[buf->btype]);
}
else
{
    runtime_error("buffer has no method '%s'", mname);
    return;
}
