#include "stdafx.h"
#include "ed.h"
#include "except.h"

ldll_module *
make_dll_module ()
{
  ldll_module *p = ldata <ldll_module, Tdll_module>::lalloc ();
  p->name = Qnil;
  p->handle = 0;
  p->loaded = 0;
  return p;
}

ldll_function *
make_dll_function ()
{
  ldll_function *p = ldata <ldll_function, Tdll_function>::lalloc ();
  p->module = Qnil;
  p->name = Qnil;
  p->proc = 0;
  p->arg_types = 0;
  p->nargs = 0;
  p->return_type = 0;
  p->arg_size = 0;
  return p;
}

static lisp
find_module (lisp name)
{
  for (lisp p = xsymbol_value (Vdll_module_list); consp (p); p = xcdr (p))
    {
      lisp x = xcar (p);
      if (dll_module_p (x)
          && xdll_module_handle (x)
          && Fstring_equalp (xdll_module_name (x), name, Qnil) != Qnil)
        return x;
    }
  return Qnil;
}

lisp
Fsi_load_dll_module (lisp lname)
{
  check_string (lname);

  lisp dll = find_module (lname);
  if (dll != Qnil)
    return dll;

  char *name = (char *)alloca (xstring_length (lname) * 2 + 1);
  w2s (name, lname);

  dll = make_dll_module ();
  lisp list = xcons (dll, xsymbol_value (Vdll_module_list));
  HMODULE h = GetModuleHandle (name);
  if (!h)
    {
      h = WINFS::LoadLibrary (name);
      if (!h)
        FEsimple_win32_error (GetLastError (), lname);
      xdll_module_loaded (dll) = 1;
    }
  xdll_module_name (dll) = lname;
  xdll_module_handle (dll) = h;
  xsymbol_value (Vdll_module_list) = list;
  return dll;
}

static u_char
check_c_type (lisp type)
{
  if (type == Kvoid)
    return CTYPE_VOID;
  if (type == Kint8)
    return CTYPE_INT8;
  if (type == Kuint8)
    return CTYPE_UINT8;
  if (type == Kint16)
    return CTYPE_INT16;
  if (type == Kuint16)
    return CTYPE_UINT16;
  if (type == Kint32)
    return CTYPE_INT32;
  if (type == Kuint32)
    return CTYPE_UINT32;
  if (type == Kint64)
    return CTYPE_INT64;
  if (type == Kuint64)
    return CTYPE_UINT64;
  if (type == Kfloat)
    return CTYPE_FLOAT;
  if (type == Kdouble)
    return CTYPE_DOUBLE;
  FEprogram_error (Eunknown_c_type, type);
  return 0;
}

static u_char
check_calling_convention (lisp keys)
{
  lisp convention = find_keyword (Kconvention, keys);
  if (convention == Qnil || convention == Kstdcall)
    return CALLING_CONVENTION_STDCALL;
  if (convention == Kcdecl)
    return CALLING_CONVENTION_CDECL;
  FEprogram_error (Eunknown_calling_convention, convention);
  return 0;
}

static int
calc_c_size (u_char type)
{
  switch (type)
    {
    case CTYPE_INT8:
    case CTYPE_UINT8:
    case CTYPE_INT16:
    case CTYPE_UINT16:
    case CTYPE_INT32:
    case CTYPE_UINT32:
      return sizeof (int);

    case CTYPE_INT64:
    case CTYPE_UINT64:
      return sizeof (int64_t);

    case CTYPE_FLOAT:
      return sizeof (float);

    case CTYPE_DOUBLE:
      return sizeof (double);

    default:
      assert (0);
    }
  return 0;
}

static u_char
check_vaarg_type (lisp type)
{
  u_char t = check_c_type (type);

  // default argument promotions
  switch (t)
    {
    case CTYPE_INT8:
    case CTYPE_INT16:
      t = CTYPE_INT32;
      break;

    case CTYPE_UINT8:
    case CTYPE_UINT16:
      t = CTYPE_UINT32;
      break;

    case CTYPE_FLOAT:
      t = CTYPE_DOUBLE;
      break;
    }

  return t;
}

static void
check_vaargs (lisp vaargs)
{
  if (vaargs == Qnil)
    return;

  if (!consp (vaargs))
    FEprogram_error (Einvalid_c_vaarg_type, vaargs);

  for (; consp (vaargs); vaargs = xcdr (vaargs))
    {
      lisp vaarg = xcar (vaargs);
      if (!consp (vaarg) || xlist_length (vaarg) != 2)
        FEprogram_error (Einvalid_c_vaarg_type, vaargs);

      u_char t = check_c_type (xcar (vaarg));
      if (t == CTYPE_VOID)
        FEprogram_error (Einvalid_c_vaarg_type, vaargs);
    }
}

static int
calc_argument_size (u_char *at, lisp largs)
{
  int size = 0;
  for (lisp a = largs; consp (a); a = xcdr (a))
    {
      u_char t = check_c_type (xcar (a));
      *at++ = t;
      size += calc_c_size (t);
    }
  return size;
}

lisp
Fsi_make_c_function (lisp lmodule, lisp lname, lisp largs, lisp lrettype, lisp keys)
{
  check_dll_module (lmodule);
  check_string (lname);

  char *name = (char *)alloca (xstring_length (lname) * 2 + 1);
  w2s (name, lname);
  FARPROC proc = GetProcAddress (xdll_module_handle (lmodule), name);
  if (!proc)
    FEsimple_win32_error (GetLastError (), lname);

  int return_type = check_c_type (lrettype);
  int nargs = 0;
  for (lisp a = largs; consp (a); a = xcdr (a), nargs++)
    if (check_c_type (xcar (a)) == CTYPE_VOID)
      FEprogram_error (Einvalid_c_argument_type, Kvoid);

  u_char vaarg_p = find_keyword_bool (Kvaarg, keys);
  if (vaarg_p && check_calling_convention (keys) == CALLING_CONVENTION_STDCALL)
    FEprogram_error (Ecannot_call_vaarg_function_by_stdcall);

  lisp fn = make_dll_function ();
  xdll_function_module (fn) = lmodule;
  xdll_function_name (fn) = lname;
  xdll_function_proc (fn) = proc;
  xdll_function_return_type (fn) = return_type;
  xdll_function_vaarg_p (fn) = vaarg_p;

  xdll_function_nargs (fn) = nargs;
  if (nargs)
    {
      u_char *at = (u_char *)xmalloc (nargs);
      xdll_function_arg_types (fn) = at;
      xdll_function_arg_size (fn) = calc_argument_size (at, largs);
    }
  return fn;
}

lisp
Fsi_last_win32_error ()
{
  return xsymbol_value (Vlast_win32_error);
}

lisp
Fsi_set_last_win32_error (lisp lerror)
{
  DWORD error = fixnum_value (lerror);
  SetLastError (error);
  xsymbol_value (Vlast_win32_error) = lerror;
  return lerror;
}

static __forceinline void
save_last_error ()
{
  xsymbol_value (Vlast_win32_error) = make_fixnum (GetLastError ());
}

// x64 has one calling convention. Arguments 1-4 go in RCX/RDX/R8/R9 or XMM0-3 by
// position, the rest on the stack. Passing every argument as a double through a
// variadic prototype makes the compiler copy a floating-point argument into the
// integer register as well, so each callee finds its argument in whichever register
// it reads. An argument travels as its 64-bit pattern (a float in the low 32 bits).
// The caller cleans up the stack, so the unused trailing slots are harmless, as in
// funcall_builtin (eval.cc).
enum { DLL_MAX_ARGS = 16 };

typedef uint64_t (*x64_proc_int) (...);
typedef double (*x64_proc_double) (...);

static void
check_x64_arg_count (int n)
{
  if (n >= DLL_MAX_ARGS)
    FEsimple_error (Ex64_ffi_too_many_args);
}

static double
x64_arg_bits (u_char type, lisp a)
{
  uint64_t bits = 0;
  switch (type)
    {
    default:
      assert (0);
    case CTYPE_INT8:
    case CTYPE_UINT8:
    case CTYPE_INT16:
    case CTYPE_UINT16:
    case CTYPE_INT32:
    case CTYPE_UINT32:
      bits = uint64_t (int64_t (cast_to_long (a)));
      break;

    case CTYPE_INT64:
    case CTYPE_UINT64:
      bits = uint64_t (cast_to_int64 (a));
      break;

    case CTYPE_FLOAT:
      {
        float f = coerce_to_single_float (a);
        uint32_t lo;
        memcpy (&lo, &f, sizeof lo);
        bits = lo;
      }
      break;

    case CTYPE_DOUBLE:
      {
        double d = coerce_to_double_float (a);
        memcpy (&bits, &d, sizeof bits);
      }
      break;
    }

  double arg;
  memcpy (&arg, &bits, sizeof arg);
  return arg;
}

static uint64_t
call_proc_x64_int (FARPROC proc, const double *args)
{
  uint64_t r = reinterpret_cast <x64_proc_int> (proc) (
    args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
    args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);
  save_last_error ();
  return r;
}

static double
call_proc_x64_double (FARPROC proc, const double *args)
{
  double r = reinterpret_cast <x64_proc_double> (proc) (
    args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
    args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);
  save_last_error ();
  return r;
}

static float
float_from_low_bits (double d)
{
  uint64_t bits;
  memcpy (&bits, &d, sizeof bits);
  uint32_t lo = uint32_t (bits);
  float f;
  memcpy (&f, &lo, sizeof f);
  return f;
}

lisp
funcall_dll (lisp fn, lisp arglist)
{
  assert (dll_function_p (fn));

  if (!xdll_function_proc (fn))
    FEprogram_error (Edll_not_initialized, fn);

  double args[DLL_MAX_ARGS] = {};
  int n = 0;
  for (const u_char *at = xdll_function_arg_types (fn),
       *ae = at + xdll_function_nargs (fn);
       at < ae; at++, arglist = xcdr (arglist))
    {
      if (!consp (arglist))
        FEtoo_few_arguments ();
      check_x64_arg_count (n);
      args[n++] = x64_arg_bits (*at, xcar (arglist));
    }

  if (consp (arglist) && xdll_function_vaarg_p (fn))
    {
      lisp vaargs = xcar (arglist);
      check_vaargs (vaargs);
      for (; consp (vaargs); vaargs = xcdr (vaargs))
        {
          lisp vaarg = xcar (vaargs);
          check_x64_arg_count (n);
          args[n++] = x64_arg_bits (check_vaarg_type (xcar (vaarg)), Fcadr (vaarg));
        }
      arglist = xcdr (arglist);
    }

  if (consp (arglist))
    FEtoo_many_arguments ();

  FARPROC proc = xdll_function_proc (fn);
  try
    {
      switch (xdll_function_return_type (fn))
        {
        default:
          assert (0);

        case CTYPE_VOID:
          call_proc_x64_int (proc, args);
          return Qnil;

        case CTYPE_INT8:
          return make_fixnum (char (call_proc_x64_int (proc, args)));

        case CTYPE_UINT8:
          return make_fixnum (u_char (call_proc_x64_int (proc, args)));

        case CTYPE_INT16:
          return make_fixnum (short (call_proc_x64_int (proc, args)));

        case CTYPE_UINT16:
          return make_fixnum (u_short (call_proc_x64_int (proc, args)));

        case CTYPE_INT32:
          return make_fixnum (long (call_proc_x64_int (proc, args)));

        case CTYPE_UINT32:
          return make_integer (u_long (call_proc_x64_int (proc, args)));

        case CTYPE_INT64:
          return make_integer (int64_t (call_proc_x64_int (proc, args)));

        case CTYPE_UINT64:
          return make_integer (call_proc_x64_int (proc, args));

        case CTYPE_FLOAT:
          return make_single_float (float_from_low_bits (call_proc_x64_double (proc, args)));

        case CTYPE_DOUBLE:
          return make_double_float (call_proc_x64_double (proc, args));
        }
    }
  catch (Win32Exception &e)
    {
      e.throw_lisp_error ();
      throw;
    }
}
