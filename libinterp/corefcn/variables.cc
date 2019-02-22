/*

Copyright (C) 1993-2019 John W. Eaton
Copyright (C) 2009-2010 VZLU Prague

This file is part of Octave.

Octave is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Octave is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Octave; see the file COPYING.  If not, see
<https://www.gnu.org/licenses/>.

*/

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <cstdio>
#include <cstring>

#include <iomanip>
#include <set>
#include <string>

#include "file-stat.h"
#include "oct-env.h"
#include "file-ops.h"
#include "glob-match.h"
#include "lo-regexp.h"
#include "str-vec.h"

#include "call-stack.h"
#include "Cell.h"
#include "defun.h"
#include "dirfns.h"
#include "error.h"
#include "errwarn.h"
#include "help.h"
#include "input.h"
#include "interpreter-private.h"
#include "interpreter.h"
#include "lex.h"
#include "load-path.h"
#include "octave-link.h"
#include "octave-preserve-stream-state.h"
#include "oct-map.h"
#include "ovl.h"
#include "ov.h"
#include "ov-class.h"
#include "ov-usr-fcn.h"
#include "pager.h"
#include "parse.h"
#include "syminfo.h"
#include "symtab.h"
#include "unwind-prot.h"
#include "utils.h"
#include "variables.h"

// Attributes of variables and functions.

// Is this octave_value a valid function?

octave_function *
is_valid_function (const std::string& fcn_name,
                   const std::string& warn_for, bool warn)
{
  octave_function *ans = nullptr;

  if (! fcn_name.empty ())
    {
      octave::symbol_table& symtab
        = octave::__get_symbol_table__ ("is_valid_function");

      octave_value val = symtab.find_function (fcn_name);

      if (val.is_defined ())
        ans = val.function_value (true);
    }

  // FIXME: Should this be "err" and "error_for", rather than warn?
  if (! ans && warn)
    error ("%s: the symbol '%s' is not valid as a function",
           warn_for.c_str (), fcn_name.c_str ());

  return ans;
}

octave_function *
is_valid_function (const octave_value& arg,
                   const std::string& warn_for, bool warn)
{
  octave_function *ans = nullptr;

  std::string fcn_name;

  if (arg.is_string ())
    {
      fcn_name = arg.string_value ();

      ans = is_valid_function (fcn_name, warn_for, warn);
    }
  else if (warn)
    // FIXME: Should this be "err" and "error_for", rather than warn?
    error ("%s: argument must be a string containing function name",
           warn_for.c_str ());

  return ans;
}

octave_function *
extract_function (const octave_value& arg, const std::string& warn_for,
                  const std::string& fname, const std::string& header,
                  const std::string& trailer)
{
  octave_function *retval = is_valid_function (arg, warn_for, 0);

  if (! retval)
    {
      std::string s = arg.xstring_value ("%s: argument must be a string",
                                         warn_for.c_str ());

      std::string cmd = header;
      cmd.append (s);
      cmd.append (trailer);

      int parse_status;

      octave::interpreter& interp
        = octave::__get_interpreter__ ("extract_function");

      interp.eval_string (cmd, true, parse_status, 0);

      if (parse_status != 0)
        error ("%s: '%s' is not valid as a function",
               warn_for.c_str (), fname.c_str ());

      retval = is_valid_function (fname, warn_for, 0);

      if (! retval)
        error ("%s: '%s' is not valid as a function",
               warn_for.c_str (), fname.c_str ());

      warning ("%s: passing function body as a string is obsolete; please use anonymous functions",
               warn_for.c_str ());
    }

  return retval;
}

static octave_value
do_isglobal (octave::symbol_table& symtab, const octave_value_list& args)
{
  if (args.length () != 1)
    print_usage ();

  if (! args(0).is_string ())
    error ("isglobal: NAME must be a string");

  octave::symbol_scope scope = symtab.current_scope ();

  std::string name = args(0).string_value ();

  return scope && scope.is_global (name);
}

DEFMETHOD (isglobal, interp, args, ,
           doc: /* -*- texinfo -*-
@deftypefn {} {} isglobal (@var{name})
Return true if @var{name} is a globally visible variable.

For example:

@example
@group
global x
isglobal ("x")
   @result{} 1
@end group
@end example
@seealso{isvarname, exist}
@end deftypefn */)
{
  octave::symbol_table& symtab = interp.get_symbol_table ();

  return do_isglobal (symtab, args);
}

/*
%!test
%! global x;
%! assert (isglobal ("x"), true);
%! clear -global x;  # cleanup after test

%!error isglobal ()
%!error isglobal ("a", "b")
%!error isglobal (1)
*/

static int
symbol_exist (octave::interpreter& interp, const std::string& name,
              const std::string& type = "any")
{
  if (octave::iskeyword (name))
    return 0;

  bool search_any = type == "any";
  bool search_var = type == "var";
  bool search_dir = type == "dir";
  bool search_file = type == "file";
  bool search_builtin = type == "builtin";
  bool search_class = type == "class";

  if (! (search_any || search_var || search_dir || search_file
         || search_builtin || search_class))
    error (R"(exist: unrecognized type argument "%s")", type.c_str ());

  octave::symbol_table& symtab = interp.get_symbol_table ();

  if (search_any || search_var)
    {
      octave::symbol_scope scope = symtab.current_scope ();

      octave_value val = scope ? scope.varval (name) : octave_value ();

      if (val.is_constant () || val.isobject ()
          || val.is_function_handle ()
          || val.is_anonymous_function ()
          || val.is_inline_function ())
        return 1;

      if (search_var)
        return 0;
    }

  // We shouldn't need to look in the global symbol table, since any name
  // that is visible in the current scope will be in the local symbol table.

  // Command line function which Matlab does not support
  if (search_any)
    {
      octave_value val = symtab.find_cmdline_function (name);

      if (val.is_defined ())
        return 103;
    }

  if (search_any || search_file || search_dir)
    {
      std::string file_name = octave::lookup_autoload (name);

      if (file_name.empty ())
        {
          octave::load_path& lp = interp.get_load_path ();

          file_name = lp.find_fcn (name);
        }

      size_t len = file_name.length ();

      if (len > 0)
        {
          if (search_any || search_file)
            {
              if (len > 4 && (file_name.substr (len-4) == ".oct"
                              || file_name.substr (len-4) == ".mex"))
                return 3;
              else
                return 2;
            }
        }

      file_name = octave::file_in_path (name, "");

      if (file_name.empty ())
        file_name = name;

      octave::sys::file_stat fs (file_name);

      if (fs)
        {
          if (search_any || search_file)
            {
              if (fs.is_dir ())
                return 7;

              len = file_name.length ();

              if (len > 4 && (file_name.substr (len-4) == ".oct"
                              || file_name.substr (len-4) == ".mex"))
                return 3;
              else
                return 2;
            }
          else if (search_dir && fs.is_dir ())
            return 7;
        }

      if (search_file || search_dir)
        return 0;
    }

  if (search_any || search_builtin)
    {
      if (symtab.is_built_in_function_name (name))
        return 5;

      if (search_builtin)
        return 0;
    }

  return 0;
}

int
symbol_exist (const std::string& name, const std::string& type)
{
  octave::interpreter& interp = octave::__get_interpreter__ ("symbol_exist");

  return symbol_exist (interp, name, type);
}


#define GET_IDX(LEN)                                                    \
  static_cast<int> (((LEN)-1) * static_cast<double> (rand ()) / RAND_MAX)

std::string
unique_symbol_name (const std::string& basename)
{
  static const std::string alpha
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  static size_t len = alpha.length ();

  std::string nm = basename + alpha[GET_IDX (len)];

  size_t pos = nm.length ();

  if (nm.substr (0, 2) == "__")
    nm.append ("__");

  octave::interpreter& interp
    = octave::__get_interpreter__ ("unique_symbol_name");

  while (symbol_exist (interp, nm, "any"))
    nm.insert (pos++, 1, alpha[GET_IDX (len)]);

  return nm;
}

DEFMETHOD (exist, interp, args, ,
           doc: /* -*- texinfo -*-
@deftypefn  {} {@var{c} =} exist (@var{name})
@deftypefnx {} {@var{c} =} exist (@var{name}, @var{type})
Check for the existence of @var{name} as a variable, function, file, directory,
or class.

The return code @var{c} is one of

@table @asis
@item 1
@var{name} is a variable.

@item 2
@var{name} is an absolute filename, an ordinary file in Octave's @code{path},
or (after appending @samp{.m}) a function file in Octave's @code{path}.

@item 3
@var{name} is a @samp{.oct} or @samp{.mex} file in Octave's @code{path}.

@item 5
@var{name} is a built-in function.

@item 7
@var{name} is a directory.

@item 8
@var{name} is a class.  (Note: not currently implemented)

@item 103
@var{name} is a function not associated with a file (entered on the command
line).

@item 0
@var{name} does not exist.
@end table

If the optional argument @var{type} is supplied, check only for symbols of the
specified type.  Valid types are

@table @asis
@item @qcode{"var"}
Check only for variables.

@item @qcode{"builtin"}
Check only for built-in functions.

@item @qcode{"dir"}
Check only for directories.

@item @qcode{"file"}
Check only for files and directories.

@item @qcode{"class"}
Check only for classes.  (Note: This option is accepted, but not currently
implemented)
@end table

If no type is given, and there are multiple possible matches for name,
@code{exist} will return a code according to the following priority list:
variable, built-in function, oct-file, directory, file, class.

@code{exist} returns 2 if a regular file called @var{name} is present in
Octave's search path.  For information about other types of files not on the
search path use some combination of the functions @code{file_in_path} and
@code{stat} instead.

Programming Note: If @var{name} is implemented by a buggy .oct/.mex file,
calling @var{exist} may cause Octave to crash.  To maintain high performance,
Octave trusts .oct/.mex files instead of @nospell{sandboxing} them.

@seealso{file_in_loadpath, file_in_path, dir_in_loadpath, stat}
@end deftypefn */)
{
  int nargin = args.length ();

  if (nargin < 1 || nargin > 2)
    print_usage ();

  std::string name = args(0).xstring_value ("exist: NAME must be a string");

  if (nargin == 2)
    {
      std::string type = args(1).xstring_value ("exist: TYPE must be a string");

      if (type == "class")
        warning (R"(exist: "class" type argument is not implemented)");

      return ovl (symbol_exist (interp, name, type));
    }
  else
    return ovl (symbol_exist (interp, name));
}

/*
%!shared dirtmp, __var1
%! dirtmp = P_tmpdir ();
%! __var1 = 1;

%!assert (exist ("__%Highly_unlikely_name%__"), 0)
%!assert (exist ("__var1"), 1)
%!assert (exist ("__var1", "var"), 1)
%!assert (exist ("__var1", "builtin"), 0)
%!assert (exist ("__var1", "dir"), 0)
%!assert (exist ("__var1", "file"), 0)

%!test
%! if (isunix ())
%!   assert (exist ("/bin/sh"), 2);
%!   assert (exist ("/bin/sh", "file"), 2);
%!   assert (exist ("/bin/sh", "dir"), 0);
%!   assert (exist ("/dev/null"), 2);
%!   assert (exist ("/dev/null", "file"), 2);
%!   assert (exist ("/dev/null", "dir"), 0);
%! endif

%!assert (exist ("print_usage"), 2)
%!assert (exist ("print_usage.m"), 2)
%!assert (exist ("print_usage", "file"), 2)
%!assert (exist ("print_usage", "dir"), 0)

## Don't search path for rooted relative filenames
%!assert (exist ("plot.m", "file"), 2)
%!assert (exist ("./plot.m", "file"), 0)
%!assert (exist ("./%nonexistentfile%", "file"), 0)
%!assert (exist ("%nonexistentfile%", "file"), 0)

## Don't search path for absolute filenames
%!test
%! tname = tempname (pwd ());
%! unwind_protect
%!   ## open/close file to create it, equivalent of touch
%!   fid = fopen (tname, "w");
%!   fclose (fid);
%!   [~, fname] = fileparts (tname);
%!   assert (exist (fullfile (pwd (), fname), "file"), 2);
%! unwind_protect_cleanup
%!   unlink (tname);
%! end_unwind_protect
%! assert (exist (fullfile (pwd (), "%nonexistentfile%"), "file"), 0);

%!testif HAVE_CHOLMOD
%! assert (exist ("chol"), 3);
%! assert (exist ("chol.oct"), 3);
%! assert (exist ("chol", "file"), 3);
%! assert (exist ("chol", "builtin"), 0);

%!assert (exist ("sin"), 5)
%!assert (exist ("sin", "builtin"), 5)
%!assert (exist ("sin", "file"), 0)

%!assert (exist (dirtmp), 7)
%!assert (exist (dirtmp, "dir"), 7)
%!assert (exist (dirtmp, "file"), 7)

%!error exist ()
%!error exist (1,2,3)
%!warning <"class" type argument is not implemented> exist ("a", "class");
%!error <TYPE must be a string> exist ("a", 1)
%!error <NAME must be a string> exist (1)
%!error <unrecognized type argument "foobar"> exist ("a", "foobar")

*/

// Variable values.

static bool
wants_local_change (const octave_value_list& args, int& nargin)
{
  bool retval = false;

  if (nargin == 2)
    {
      if (! args(1).is_string () || args(1).string_value () != "local")
        error_with_cfn (R"(second argument must be "local")");

      nargin = 1;
      retval = true;
    }

  return retval;
}

static octave::unwind_protect *
curr_fcn_unwind_protect_frame (void)
{
  octave::call_stack& cs
    = octave::__get_call_stack__ ("curr_fcn_unwind_protect_frame");

  return cs.curr_fcn_unwind_protect_frame ();
}

template <typename T>
static bool
try_local_protect (T& var)
{
  octave::unwind_protect *frame = curr_fcn_unwind_protect_frame ();

  if (frame)
    {
      frame->protect_var (var);
      return true;
    }
  else
    return false;
}

octave_value
set_internal_variable (bool& var, const octave_value_list& args,
                       int nargout, const char *nm)
{
  octave_value retval;

  int nargin = args.length ();

  if (nargout > 0 || nargin == 0)
    retval = var;

  if (wants_local_change (args, nargin))
    {
      if (! try_local_protect (var))
        warning (R"("local" has no effect outside a function)");
    }

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      bool bval = args(0).xbool_value ("%s: argument must be a logical value", nm);

      var = bval;
    }

  return retval;
}

octave_value
set_internal_variable (char& var, const octave_value_list& args,
                       int nargout, const char *nm)
{
  octave_value retval;

  int nargin = args.length ();

  if (nargout > 0 || nargin == 0)
    retval = var;

  if (wants_local_change (args, nargin))
    {
      if (! try_local_protect (var))
        warning (R"("local" has no effect outside a function)");
    }

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      std::string sval = args(0).xstring_value ("%s: argument must be a single character", nm);

      switch (sval.length ())
        {
        case 1:
          var = sval[0];
          break;

        case 0:
          var = '\0';
          break;

        default:
          error ("%s: argument must be a single character", nm);
          break;
        }
    }

  return retval;
}

octave_value
set_internal_variable (int& var, const octave_value_list& args,
                       int nargout, const char *nm,
                       int minval, int maxval)
{
  octave_value retval;

  int nargin = args.length ();

  if (nargout > 0 || nargin == 0)
    retval = var;

  if (wants_local_change (args, nargin))
    {
      if (! try_local_protect (var))
        warning (R"("local" has no effect outside a function)");
    }

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      int ival = args(0).xint_value ("%s: argument must be an integer value", nm);

      if (ival < minval)
        error ("%s: arg must be greater than %d", nm, minval);
      if (ival > maxval)
        error ("%s: arg must be less than or equal to %d", nm, maxval);

      var = ival;
    }

  return retval;
}

octave_value
set_internal_variable (double& var, const octave_value_list& args,
                       int nargout, const char *nm,
                       double minval, double maxval)
{
  octave_value retval;

  int nargin = args.length ();

  if (nargout > 0 || nargin == 0)
    retval = var;

  if (wants_local_change (args, nargin))
    {
      if (! try_local_protect (var))
        warning (R"("local" has no effect outside a function)");
    }

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      double dval = args(0).xscalar_value ("%s: argument must be a scalar value", nm);

      if (dval < minval)
        error ("%s: argument must be greater than %g", nm, minval);
      if (dval > maxval)
        error ("%s: argument must be less than or equal to %g", nm, maxval);

      var = dval;
    }

  return retval;
}

octave_value
set_internal_variable (std::string& var, const octave_value_list& args,
                       int nargout, const char *nm, bool empty_ok)
{
  octave_value retval;

  int nargin = args.length ();

  if (nargout > 0 || nargin == 0)
    retval = var;

  if (wants_local_change (args, nargin))
    {
      if (! try_local_protect (var))
        warning (R"("local" has no effect outside a function)");
    }

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      std::string sval = args(0).xstring_value ("%s: first argument must be a string", nm);

      if (! empty_ok && sval.empty ())
        error ("%s: value must not be empty", nm);

      var = sval;
    }

  return retval;
}

octave_value
set_internal_variable (int& var, const octave_value_list& args,
                       int nargout, const char *nm, const char **choices)
{
  octave_value retval;
  int nchoices = 0;
  while (choices[nchoices] != nullptr)
    nchoices++;

  int nargin = args.length ();

  assert (var < nchoices);

  if (nargout > 0 || nargin == 0)
    retval = choices[var];

  if (wants_local_change (args, nargin))
    {
      if (! try_local_protect (var))
        warning (R"("local" has no effect outside a function)");
    }

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      std::string sval = args(0).xstring_value ("%s: first argument must be a string", nm);

      int i = 0;
      for (; i < nchoices; i++)
        {
          if (sval == choices[i])
            {
              var = i;
              break;
            }
        }
      if (i == nchoices)
        error (R"(%s: value not allowed ("%s"))", nm, sval.c_str ());
    }

  return retval;
}

octave_value
set_internal_variable (std::string& var, const octave_value_list& args,
                       int nargout, const char *nm, const char **choices)
{
  octave_value retval;
  int nchoices = 0;
  while (choices[nchoices] != nullptr)
    nchoices++;

  int nargin = args.length ();

  if (nargout > 0 || nargin == 0)
    retval = var;

  if (wants_local_change (args, nargin))
    {
      if (! try_local_protect (var))
        warning (R"("local" has no effect outside a function)");
    }

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      std::string sval = args(0).xstring_value ("%s: first argument must be a string", nm);

      int i = 0;
      for (; i < nchoices; i++)
        {
          if (sval == choices[i])
            {
              var = sval;
              break;
            }
        }
      if (i == nchoices)
        error (R"(%s: value not allowed ("%s"))", nm, sval.c_str ());
    }

  return retval;
}

DEFMETHOD (mlock, interp, args, ,
           doc: /* -*- texinfo -*-
@deftypefn {} {} mlock ()
Lock the current function into memory so that it can't be removed with
@code{clear}.
@seealso{munlock, mislocked, persistent, clear}
@end deftypefn */)
{
  if (args.length () != 0)
    print_usage ();

  octave::call_stack& cs = interp.get_call_stack ();

  octave_function *fcn = cs.caller ();

  if (! fcn)
    error ("mlock: invalid use outside a function");

  fcn->lock ();

  return ovl ();
}

DEFMETHOD (munlock, interp, args, ,
           doc: /* -*- texinfo -*-
@deftypefn  {} {} munlock ()
@deftypefnx {} {} munlock (@var{fcn})
Unlock the named function @var{fcn} so that it may be removed from memory with
@code{clear}.

If no function is named then unlock the current function.
@seealso{mlock, mislocked, persistent, clear}
@end deftypefn */)
{
  int nargin = args.length ();

  if (nargin > 1)
    print_usage ();

  if (nargin == 1)
    {
      std::string name = args(0).xstring_value ("munlock: FCN must be a string");

      interp.munlock (name);
    }
  else
    {
      octave::call_stack& cs = interp.get_call_stack ();

      octave_function *fcn = cs.caller ();

      if (! fcn)
        error ("munlock: invalid use outside a function");

      fcn->unlock ();
    }

  return ovl ();
}

DEFMETHOD (mislocked, interp, args, ,
           doc: /* -*- texinfo -*-
@deftypefn  {} {} mislocked ()
@deftypefnx {} {} mislocked (@var{fcn})
Return true if the named function @var{fcn} is locked in memory.

If no function is named then return true if the current function is locked.
@seealso{mlock, munlock, persistent, clear}
@end deftypefn */)
{
  int nargin = args.length ();

  if (nargin > 1)
    print_usage ();

  octave_value retval;

  if (nargin == 1)
    {
      std::string name = args(0).xstring_value ("mislocked: FCN must be a string");

      retval = interp.mislocked (name);
    }
  else
    {
      octave::call_stack& cs = interp.get_call_stack ();

      octave_function *fcn = cs.caller ();

      if (! fcn)
        error ("mislocked: invalid use outside a function");

      retval = fcn->islocked ();
    }

  return retval;
}

// Deleting names from the symbol tables.

static inline bool
name_matches_any_pattern (const std::string& nm, const string_vector& argv,
                          int argc, int idx, bool have_regexp = false)
{
  bool retval = false;

  for (int k = idx; k < argc; k++)
    {
      std::string patstr = argv[k];
      if (! patstr.empty ())
        {
          if (have_regexp)
            {
              if (octave::regexp::is_match (patstr, nm))
                {
                  retval = true;
                  break;
                }
            }
          else
            {
              glob_match pattern (patstr);

              if (pattern.match (nm))
                {
                  retval = true;
                  break;
                }
            }
        }
    }

  return retval;
}

static inline void
maybe_warn_exclusive (bool exclusive)
{
  if (exclusive)
    warning ("clear: ignoring --exclusive option");
}

static void
do_clear_functions (octave::symbol_table& symtab,
                    const string_vector& argv, int argc, int idx,
                    bool exclusive = false)
{
  if (idx == argc)
    symtab.clear_functions ();
  else
    {
      if (exclusive)
        {
          string_vector fcns = symtab.user_function_names ();

          int fcount = fcns.numel ();

          for (int i = 0; i < fcount; i++)
            {
              std::string nm = fcns[i];

              if (! name_matches_any_pattern (nm, argv, argc, idx))
                symtab.clear_function (nm);
            }
        }
      else
        {
          while (idx < argc)
            symtab.clear_function_pattern (argv[idx++]);
        }
    }
}

static void
do_clear_globals (octave::symbol_table& symtab,
                  const string_vector& argv, int argc, int idx,
                  bool exclusive = false)
{
  octave::symbol_scope scope = symtab.current_scope ();

  if (! scope)
    return;

  if (idx == argc)
    {
      string_vector gvars = symtab.global_variable_names ();

      int gcount = gvars.numel ();

      for (int i = 0; i < gcount; i++)
        {
          std::string name = gvars[i];

          scope.clear_variable (name);
          symtab.clear_global (name);
        }
    }
  else
    {
      if (exclusive)
        {
          string_vector gvars = symtab.global_variable_names ();

          int gcount = gvars.numel ();

          for (int i = 0; i < gcount; i++)
            {
              std::string name = gvars[i];

              if (! name_matches_any_pattern (name, argv, argc, idx))
                {
                  scope.clear_variable (name);
                  symtab.clear_global (name);
                }
            }
        }
      else
        {
          while (idx < argc)
            {
              std::string pattern = argv[idx++];

              scope.clear_variable_pattern (pattern);
              symtab.clear_global_pattern (pattern);
            }
        }
    }
}

static void
do_clear_variables (octave::symbol_table& symtab,
                    const string_vector& argv, int argc, int idx,
                    bool exclusive = false, bool have_regexp = false)
{
  octave::symbol_scope scope = symtab.current_scope ();

  if (! scope)
    return;

  if (idx == argc)
    scope.clear_variables ();
  else
    {
      if (exclusive)
        {
          string_vector lvars = scope.variable_names ();

          int lcount = lvars.numel ();

          for (int i = 0; i < lcount; i++)
            {
              std::string nm = lvars[i];

              if (! name_matches_any_pattern (nm, argv, argc, idx, have_regexp))
                scope.clear_variable (nm);
            }
        }
      else
        {
          if (have_regexp)
            while (idx < argc)
              scope.clear_variable_regexp (argv[idx++]);
          else
            while (idx < argc)
              scope.clear_variable_pattern (argv[idx++]);
        }
    }
}

static void
do_clear_symbols (octave::symbol_table& symtab,
                  const string_vector& argv, int argc, int idx,
                  bool exclusive = false)
{
  if (idx == argc)
    {
      octave::symbol_scope scope = symtab.current_scope ();

      if (scope)
        scope.clear_variables ();
    }
  else
    {
      if (exclusive)
        {
          // FIXME: is this really what we want, or do we
          // somehow want to only clear the functions that are not
          // shadowed by local variables?  It seems that would be a
          // bit harder to do.

          do_clear_variables (symtab, argv, argc, idx, exclusive);
          do_clear_functions (symtab, argv, argc, idx, exclusive);
        }
      else
        {
          while (idx < argc)
            symtab.clear_symbol_pattern (argv[idx++]);
        }
    }
}

static void
do_matlab_compatible_clear (octave::symbol_table& symtab,
                            const string_vector& argv, int argc, int idx)
{
  // This is supposed to be mostly Matlab compatible.

  octave::symbol_scope scope = symtab.current_scope ();

  if (! scope)
    return;

  for (; idx < argc; idx++)
    {
      if (argv[idx] == "all"
          && ! scope.is_local_variable ("all"))
        {
          symtab.clear_all ();
        }
      else if (argv[idx] == "functions"
               && ! scope.is_local_variable ("functions"))
        {
          do_clear_functions (symtab, argv, argc, ++idx);
        }
      else if (argv[idx] == "global"
               && ! scope.is_local_variable ("global"))
        {
          do_clear_globals (symtab, argv, argc, ++idx);
        }
      else if (argv[idx] == "variables"
               && ! scope.is_local_variable ("variables"))
        {
          scope.clear_variables ();
        }
      else if (argv[idx] == "classes"
               && ! scope.is_local_variable ("classes"))
        {
          scope.clear_objects ();
          octave_class::clear_exemplar_map ();
          symtab.clear_all ();
        }
      else
        {
          symtab.clear_symbol_pattern (argv[idx]);
        }
    }
}

DEFMETHOD (clear, interp, args, ,
           doc: /* -*- texinfo -*-
@deftypefn  {} {} clear
@deftypefnx {} {} clear @var{pattern} @dots{}
@deftypefnx {} {} clear @var{options} @var{pattern} @dots{}
Delete the names matching the given @var{pattern}s from the symbol table.

The @var{pattern} may contain the following special characters:

@table @code
@item ?
Match any single character.

@item *
Match zero or more characters.

@item [ @var{list} ]
Match the list of characters specified by @var{list}.  If the first character
is @code{!} or @code{^}, match all characters except those specified by
@var{list}.  For example, the pattern @code{[a-zA-Z]} will match all lowercase
and uppercase alphabetic characters.
@end table

For example, the command

@example
clear foo b*r
@end example

@noindent
clears the name @code{foo} and all names that begin with the letter @samp{b}
and end with the letter @samp{r}.

If @code{clear} is called without any arguments, all user-defined variables
are cleared from the current workspace (i.e., local variables).  Any global
variables present will no longer be visible in the current workspace, but they
will continue to exist in the global workspace.  Functions are unaffected by
this form of @code{clear}.

The following options are available in both long and short form

@table @code
@item all, -all, -a
Clear all local and global user-defined variables, and all functions from the
symbol table.

@item -exclusive, -x
Clear variables that do @strong{not} match the following pattern.

@item functions, -functions, -f
Clear function names from the function symbol table.  Persistent variables
will be re-initialized to their default value unless the function has been
locked in memory with @code{mlock}.

@item global, -global, -g
Clear global variable names.

@item variables, -variables, -v
Clear local variable names.

@item classes, -classes, -c
Clear the class structure table and all objects.

@item -regexp, -r
The @var{pattern} arguments are treated as regular expressions and any matches
will be cleared.
@end table

With the exception of @option{-exclusive} and @option{-regexp}, all long
options can be used without the dash as well.  Note that, aside from
@option{-exclusive}, only one other option may appear.  All options must
appear before any patterns.

Programming Note: The command @code{clear @var{name}} only clears the variable
@var{name} when both a variable and a (shadowed) function named @var{name}
are currently defined.  For example, suppose you have defined a function
@code{foo}, and then hidden it by performing the assignment @code{foo = 2}.
Executing the command @code{clear foo} once will clear the variable
definition and restore the definition of @code{foo} as a function.
Executing @code{clear foo} a second time will clear the function definition.

@seealso{who, whos, exist, mlock}
@end deftypefn */)
{
  octave::symbol_table& symtab = interp.get_symbol_table ();

  int argc = args.length () + 1;

  string_vector argv = args.make_argv ("clear");

  if (argc == 1)
    {
      do_clear_variables (symtab, argv, argc, argc);

      octave_link::clear_workspace ();
    }
  else
    {
      int idx = 0;

      bool clear_all = false;
      bool clear_functions = false;
      bool clear_globals = false;
      bool clear_variables = false;
      bool clear_objects = false;
      bool exclusive = false;
      bool have_regexp = false;
      bool have_dash_option = false;

      octave::symbol_scope scope = symtab.current_scope ();

      while (++idx < argc)
        {
          if (argv[idx] == "-all" || argv[idx] == "-a")
            {
              if (have_dash_option)
                print_usage ();

              have_dash_option = true;
              clear_all = true;
            }
          else if (argv[idx] == "-exclusive" || argv[idx] == "-x")
            {
              exclusive = true;
            }
          else if (argv[idx] == "-functions" || argv[idx] == "-f")
            {
              if (have_dash_option)
                print_usage ();

              have_dash_option = true;
              clear_functions = true;
            }
          else if (argv[idx] == "-global" || argv[idx] == "-g")
            {
              if (have_dash_option)
                print_usage ();

              have_dash_option = true;
              clear_globals = true;
            }
          else if (argv[idx] == "-variables" || argv[idx] == "-v")
            {
              if (have_dash_option)
                print_usage ();

              have_dash_option = true;
              clear_variables = true;
            }
          else if (argv[idx] == "-classes" || argv[idx] == "-c")
            {
              if (have_dash_option)
                print_usage ();

              have_dash_option = true;
              clear_objects = true;
            }
          else if (argv[idx] == "-regexp" || argv[idx] == "-r")
            {
              if (have_dash_option)
                print_usage ();

              have_dash_option = true;
              have_regexp = true;
            }
          else
            break;
        }

      if (idx <= argc)
        {
          if (! have_dash_option && ! exclusive)
            do_matlab_compatible_clear (symtab, argv, argc, idx);
          else
            {
              if (clear_all)
                {
                  maybe_warn_exclusive (exclusive);

                  if (++idx < argc)
                    warning ("clear: ignoring extra arguments after -all");

                  symtab.clear_all ();
                }
              else if (have_regexp)
                {
                  do_clear_variables (symtab, argv, argc, idx, exclusive, true);
                }
              else if (clear_functions)
                {
                  do_clear_functions (symtab, argv, argc, idx, exclusive);
                }
              else if (clear_globals)
                {
                  do_clear_globals (symtab, argv, argc, idx, exclusive);
                }
              else if (clear_variables)
                {
                  do_clear_variables (symtab, argv, argc, idx, exclusive);
                }
              else if (clear_objects)
                {
                  if (scope)
                    scope.clear_objects ();
                  octave_class::clear_exemplar_map ();
                  symtab.clear_all ();
                }
              else
                {
                  do_clear_symbols (symtab, argv, argc, idx, exclusive);
                }
            }
        }
    }

  return ovl ();
}

/*
## This test must be wrapped in its own function or the 'clear' command will
## break the %!test environment.
%!function __test_clear_no_args__ ()
%!  global x
%!  x = 3;
%!  clear
%!  assert (! exist ("x", "var"));  # x is not in the current workspace anymore
%!  global x                        # but still lives in the global workspace
%!  assert (exist ("x", "var"));
%!endfunction

%!test
%! __test_clear_no_args__ ();

## Test that multiple options cannot be given
%!error clear -f -g
*/

static std::string Vmissing_function_hook = "__unimplemented__";

DEFUN (missing_function_hook, args, nargout,
       doc: /* -*- texinfo -*-
@deftypefn  {} {@var{val} =} missing_function_hook ()
@deftypefnx {} {@var{old_val} =} missing_function_hook (@var{new_val})
@deftypefnx {} {} missing_function_hook (@var{new_val}, "local")
Query or set the internal variable that specifies the function to call when
an unknown identifier is requested.

When called from inside a function with the @qcode{"local"} option, the
variable is changed locally for the function and any subroutines it calls.
The original variable value is restored when exiting the function.
@seealso{missing_component_hook}
@end deftypefn */)
{
  return SET_INTERNAL_VARIABLE (missing_function_hook);
}

void
maybe_missing_function_hook (const std::string& name)
{
  // Don't do this if we're handling errors.
  if (buffer_error_messages == 0 && ! Vmissing_function_hook.empty ())
    {
      octave::symbol_table& symtab
        = octave::__get_symbol_table__ ("maybe_missing_function_hook");

      octave_value val = symtab.find_function (Vmissing_function_hook);

      if (val.is_defined ())
        {
          // Ensure auto-restoration.
          octave::unwind_protect frame;
          frame.protect_var (Vmissing_function_hook);

          // Clear the variable prior to calling the function.
          const std::string func_name = Vmissing_function_hook;
          Vmissing_function_hook.clear ();

          // Call.
          octave::feval (func_name, octave_value (name));
        }
    }
}

DEFMETHOD (__varval__, interp, args, ,
           doc: /* -*- texinfo -*-
@deftypefn {} {} __varval__ (@var{name})
Return the value of the variable @var{name} directly from the symbol table.
@end deftypefn */)
{
  if (args.length () != 1)
    print_usage ();

  std::string name = args(0).xstring_value ("__varval__: first argument must be a variable name");

  octave::symbol_scope scope = interp.get_current_scope ();

  return scope ? scope.varval (args(0).string_value ()) : octave_value ();
}

static std::string Vmissing_component_hook;

DEFUN (missing_component_hook, args, nargout,
       doc: /* -*- texinfo -*-
@deftypefn  {} {@var{val} =} missing_component_hook ()
@deftypefnx {} {@var{old_val} =} missing_component_hook (@var{new_val})
@deftypefnx {} {} missing_component_hook (@var{new_val}, "local")
Query or set the internal variable that specifies the function to call when
a component of Octave is missing.

This can be useful for packagers that may split the Octave installation into
multiple sub-packages, for example, to provide a hint to users for how to
install the missing components.

When called from inside a function with the @qcode{"local"} option, the
variable is changed locally for the function and any subroutines it calls.
The original variable value is restored when exiting the function.

The hook function is expected to be of the form

@example
@var{fcn} (@var{component})
@end example

Octave will call @var{fcn} with the name of the function that requires the
component and a string describing the missing component.  The hook function
should return an error message to be displayed.
@seealso{missing_function_hook}
@end deftypefn */)
{
  return SET_INTERNAL_VARIABLE (missing_component_hook);
}

// The following functions are deprecated.

void
mlock (void)
{
  octave::interpreter& interp = octave::__get_interpreter__ ("mlock");

  interp.mlock ();
}

void
munlock (const std::string& nm)
{
  octave::interpreter& interp = octave::__get_interpreter__ ("mlock");

  return interp.munlock (nm);
}

bool
mislocked (const std::string& nm)
{
  octave::interpreter& interp = octave::__get_interpreter__ ("mlock");

  return interp.mislocked (nm);
}

void
bind_ans (const octave_value& val, bool print)
{
  octave::tree_evaluator& tw = octave::__get_evaluator__ ("bind_ans");

  tw.bind_ans (val, print);
}

void
clear_mex_functions (void)
{
  octave::symbol_table& symtab =
    octave::__get_symbol_table__ ("clear_mex_functions");

  symtab.clear_mex_functions ();
}

void
clear_function (const std::string& nm)
{
  octave::symbol_table& symtab = octave::__get_symbol_table__ ("clear_function");

  symtab.clear_function (nm);
}

void
clear_variable (const std::string& nm)
{
  octave::symbol_scope scope
    = octave::__get_current_scope__ ("clear_variable");

  if (scope)
    scope.clear_variable (nm);
}

void
clear_symbol (const std::string& nm)
{
  octave::symbol_table& symtab = octave::__get_symbol_table__ ("clear_symbol");

  symtab.clear_symbol (nm);
}

octave_value
lookup_function_handle (const std::string& nm)
{
  octave::symbol_scope scope
    = octave::__get_current_scope__ ("lookup_function_handle");

  octave_value val = scope ? scope.varval (nm) : octave_value ();

  return val.is_function_handle () ? val : octave_value ();
}

octave_value
get_global_value (const std::string& nm, bool silent)
{
  octave::symbol_table& symtab =
    octave::__get_symbol_table__ ("get_global_value");

  octave_value val = symtab.global_varval (nm);

  if (val.is_undefined () && ! silent)
    error ("get_global_value: undefined symbol '%s'", nm.c_str ());

  return val;
}

void
set_global_value (const std::string& nm, const octave_value& val)
{
  octave::symbol_table& symtab =
    octave::__get_symbol_table__ ("set_global_value");

  symtab.global_assign (nm, val);
}

octave_value
get_top_level_value (const std::string& nm, bool silent)
{
  octave::symbol_table& symtab =
    octave::__get_symbol_table__ ("get_top_level_value");

  octave_value val = symtab.top_level_varval (nm);

  if (val.is_undefined () && ! silent)
    error ("get_top_level_value: undefined symbol '%s'", nm.c_str ());

  return val;
}

void
set_top_level_value (const std::string& nm, const octave_value& val)
{
  octave::symbol_table& symtab =
    octave::__get_symbol_table__ ("set_top_level_value");

  symtab.top_level_assign (nm, val);
}

string_vector
get_struct_elts (const std::string& text)
{
  int n = 1;

  size_t pos = 0;

  size_t len = text.length ();

  while ((pos = text.find ('.', pos)) != std::string::npos)
    {
      if (++pos == len)
        break;

      n++;
    }

  string_vector retval (n);

  pos = 0;

  for (int i = 0; i < n; i++)
    {
      len = text.find ('.', pos);

      if (len != std::string::npos)
        len -= pos;

      retval[i] = text.substr (pos, len);

      if (len != std::string::npos)
        pos += len + 1;
    }

  return retval;
}
