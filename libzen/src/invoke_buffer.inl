/*
** invoke_buffer.inl — Buffer method dispatch for OP_INVOKE.
** Minimal API — raw speed via buf[i] / buf[i]=x is the primary interface.
*/

ObjBuffer *buf = as_buffer(receiver);

switch (method->buffer_method_id)
{
case BUFFER_LEN:
{
    R[base] = val_int(buf->count);
    break;
}
case BUFFER_FILL:
{
    if (arg_count != 1)
    {
        RT_ERROR("fill() expects 1 argument");
    }
    double v = 0;
    if (is_int(args[0]))
        v = (double)args[0].as.integer;
    else if (is_float(args[0]))
        v = args[0].as.number;
    else
    {
        RT_ERROR("fill() expects a number");
    }
    buffer_fill(buf, v);
    R[base] = receiver;
    break;
}
case BUFFER_BYTE_LEN:
{
    R[base] = val_int(buf->count * buffer_elem_size[buf->btype]);
    break;
}
default:
{
    RT_ERROR("buffer has no method '%s'", mname);
}
}
