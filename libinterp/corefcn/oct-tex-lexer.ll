/* DO NOT EDIT. AUTOMATICALLY GENERATED FROM oct-tex-lexer.in.ll and oct-tex-symbols.in. */
/*

Copyright (C) 2013-2019 Michael Goffioul

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

%top {
#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#if defined (HAVE_PRAGMA_GCC_DIAGNOSTIC)
   // This one needs to be global.
#  pragma GCC diagnostic ignored "-Wunused-function"

   // Disable these warnings for code that is generated by flex, including
   // pattern rules.  Push the current state so we can restore the warning
   // state prior to functions we define at the bottom of the file.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsign-compare"
#  pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

// Define away the deprecated register storage class specifier to avoid
// potential warnings about it.
#if ! defined (register)
#  define register
#endif

}

%option prefix = "octave_tex_"
%option noyywrap
%option reentrant
%option bison-bridge

%option noyyalloc
%option noyyrealloc
%option noyyfree

%x NUM_MODE
%x MAYBE_NUM_MODE

%{

#include "unistd-wrappers.h"

#include "text-engine.h"

// oct-tex-parser.h must be included after text-engine.h
#include "oct-tex-parser.h"

// FIXME: with bison 3.x, OCTAVE_TEX_STYPE appears in the generated
// oct-parse.h file, but there is no definition for YYSTYPE, which is
// needed by the code that is generated by flex.  I can't seem to find
// a way to tell flex to use OCTAVE_TEX_STYPE instead of YYSTYPE in
// the code it generates, or to tell bison to provide the definition
// of YYSTYPE in the generated oct-parse.h file.

#if defined (OCTAVE_TEX_STYPE_IS_DECLARED) && ! defined YYSTYPE
#  define YYSTYPE OCTAVE_TEX_STYPE
#endif

#define YY_NO_UNISTD_H 1
#define isatty octave_isatty_wrapper
#define yyguts_t octave_tex_yyguts_t

%}

D   [0-9]
NUM (({D}+\.?{D}*)|(\.{D}+))

%%

%{
// Numeric values.
%}

<NUM_MODE>{NUM} {
    int nread = sscanf (yytext, "%lf", &(yylval->num));

    if (nread == 1)
      return NUM;
  }

<NUM_MODE>[, \t]+ { }

<NUM_MODE>"\n"|. {
    yyless (0);
    BEGIN (INITIAL);
  }

<MAYBE_NUM_MODE>"{" {
    BEGIN (NUM_MODE);
    return START;
  }

<MAYBE_NUM_MODE>"\n"|. {
    yyless (0);
    BEGIN (INITIAL);
  }

%{
// Simple commands.
%}

"\\bf" { return BF; }
"\\it" { return IT; }
"\\sl" { return SL; }
"\\rm" { return RM; }

%{
// Generic font commands.
%}

"\\fontname" { return FONTNAME; }

"\\fontsize" {
    BEGIN (MAYBE_NUM_MODE);
    return FONTSIZE;
  }

"\\color[rgb]" {
    BEGIN (MAYBE_NUM_MODE);
    return COLOR_RGB;
  }

"\\color" { return COLOR; }

%{
// Special characters.
%}

"{" { return START; }
"}" { return END; }
"^" { return SUPER; }
"_" { return SUB; }

"\\{"  |
"\\}"  |
"\\^"  |
"\\_"  |
"\\\\" {
    yylval->ch = yytext[1];
    return CH;
  }

%{
// Symbols.
%}

"\\alpha" { yylval->sym = 0; return SYM; }
"\\beta" { yylval->sym = 1; return SYM; }
"\\gamma" { yylval->sym = 2; return SYM; }
"\\delta" { yylval->sym = 3; return SYM; }
"\\epsilon" { yylval->sym = 4; return SYM; }
"\\zeta" { yylval->sym = 5; return SYM; }
"\\eta" { yylval->sym = 6; return SYM; }
"\\theta" { yylval->sym = 7; return SYM; }
"\\vartheta" { yylval->sym = 8; return SYM; }
"\\iota" { yylval->sym = 9; return SYM; }
"\\kappa" { yylval->sym = 10; return SYM; }
"\\lambda" { yylval->sym = 11; return SYM; }
"\\mu" { yylval->sym = 12; return SYM; }
"\\nu" { yylval->sym = 13; return SYM; }
"\\xi" { yylval->sym = 14; return SYM; }
"\\o" { yylval->sym = 15; return SYM; }
"\\pi" { yylval->sym = 16; return SYM; }
"\\varpi" { yylval->sym = 17; return SYM; }
"\\rho" { yylval->sym = 18; return SYM; }
"\\sigma" { yylval->sym = 19; return SYM; }
"\\varsigma" { yylval->sym = 20; return SYM; }
"\\tau" { yylval->sym = 21; return SYM; }
"\\upsilon" { yylval->sym = 22; return SYM; }
"\\phi" { yylval->sym = 23; return SYM; }
"\\chi" { yylval->sym = 24; return SYM; }
"\\psi" { yylval->sym = 25; return SYM; }
"\\omega" { yylval->sym = 26; return SYM; }
"\\Gamma" { yylval->sym = 27; return SYM; }
"\\Delta" { yylval->sym = 28; return SYM; }
"\\Theta" { yylval->sym = 29; return SYM; }
"\\Lambda" { yylval->sym = 30; return SYM; }
"\\Xi" { yylval->sym = 31; return SYM; }
"\\Pi" { yylval->sym = 32; return SYM; }
"\\Sigma" { yylval->sym = 33; return SYM; }
"\\Upsilon" { yylval->sym = 34; return SYM; }
"\\Phi" { yylval->sym = 35; return SYM; }
"\\Psi" { yylval->sym = 36; return SYM; }
"\\Omega" { yylval->sym = 37; return SYM; }
"\\aleph" { yylval->sym = 38; return SYM; }
"\\wp" { yylval->sym = 39; return SYM; }
"\\Re" { yylval->sym = 40; return SYM; }
"\\Im" { yylval->sym = 41; return SYM; }
"\\partial" { yylval->sym = 42; return SYM; }
"\\infty" { yylval->sym = 43; return SYM; }
"\\prime" { yylval->sym = 44; return SYM; }
"\\nabla" { yylval->sym = 45; return SYM; }
"\\surd" { yylval->sym = 46; return SYM; }
"\\angle" { yylval->sym = 47; return SYM; }
"\\forall" { yylval->sym = 48; return SYM; }
"\\exists" { yylval->sym = 49; return SYM; }
"\\neg" { yylval->sym = 50; return SYM; }
"\\clubsuit" { yylval->sym = 51; return SYM; }
"\\diamondsuit" { yylval->sym = 52; return SYM; }
"\\heartsuit" { yylval->sym = 53; return SYM; }
"\\spadesuit" { yylval->sym = 54; return SYM; }
"\\int" { yylval->sym = 55; return SYM; }
"\\pm" { yylval->sym = 56; return SYM; }
"\\cdot" { yylval->sym = 57; return SYM; }
"\\times" { yylval->sym = 58; return SYM; }
"\\ast" { yylval->sym = 59; return SYM; }
"\\circ" { yylval->sym = 60; return SYM; }
"\\bullet" { yylval->sym = 61; return SYM; }
"\\div" { yylval->sym = 62; return SYM; }
"\\cap" { yylval->sym = 63; return SYM; }
"\\cup" { yylval->sym = 64; return SYM; }
"\\vee" { yylval->sym = 65; return SYM; }
"\\wedge" { yylval->sym = 66; return SYM; }
"\\oplus" { yylval->sym = 67; return SYM; }
"\\otimes" { yylval->sym = 68; return SYM; }
"\\oslash" { yylval->sym = 69; return SYM; }
"\\leq" { yylval->sym = 70; return SYM; }
"\\subset" { yylval->sym = 71; return SYM; }
"\\subseteq" { yylval->sym = 72; return SYM; }
"\\in" { yylval->sym = 73; return SYM; }
"\\geq" { yylval->sym = 74; return SYM; }
"\\supset" { yylval->sym = 75; return SYM; }
"\\supseteq" { yylval->sym = 76; return SYM; }
"\\ni" { yylval->sym = 77; return SYM; }
"\\mid" { yylval->sym = 78; return SYM; }
"\\equiv" { yylval->sym = 79; return SYM; }
"\\sim" { yylval->sym = 80; return SYM; }
"\\approx" { yylval->sym = 81; return SYM; }
"\\cong" { yylval->sym = 82; return SYM; }
"\\propto" { yylval->sym = 83; return SYM; }
"\\perp" { yylval->sym = 84; return SYM; }
"\\leftarrow" { yylval->sym = 85; return SYM; }
"\\Leftarrow" { yylval->sym = 86; return SYM; }
"\\rightarrow" { yylval->sym = 87; return SYM; }
"\\Rightarrow" { yylval->sym = 88; return SYM; }
"\\leftrightarrow" { yylval->sym = 89; return SYM; }
"\\uparrow" { yylval->sym = 90; return SYM; }
"\\downarrow" { yylval->sym = 91; return SYM; }
"\\lfloor" { yylval->sym = 92; return SYM; }
"\\langle" { yylval->sym = 93; return SYM; }
"\\lceil" { yylval->sym = 94; return SYM; }
"\\rfloor" { yylval->sym = 95; return SYM; }
"\\rangle" { yylval->sym = 96; return SYM; }
"\\rceil" { yylval->sym = 97; return SYM; }
"\\neq" { yylval->sym = 98; return SYM; }
"\\ldots" { yylval->sym = 99; return SYM; }
"\\0" { yylval->sym = 100; return SYM; }
"\\copyright" { yylval->sym = 101; return SYM; }
"\\deg" { yylval->sym = 102; return SYM; }

%{
// Generic character.
%}

"\n" |
.    {
    yylval->ch = yytext[0];
    return CH;
  }

%{
#if defined (HAVE_PRAGMA_GCC_DIAGNOSTIC)
   // Also disable this warning for functions that are generated by flex
   // after the pattern rules.
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
%}

%%

#if defined (HAVE_PRAGMA_GCC_DIAGNOSTIC)
   // Restore prevailing warning state for remainder of the file.
#  pragma GCC diagnostic pop
#endif

void *
octave_tex_alloc (yy_size_t size, yyscan_t)
{
  return malloc (size);
}

void *
octave_tex_realloc (void *ptr, yy_size_t size, yyscan_t)
{
return realloc (ptr, size);
}

void
octave_tex_free (void *ptr, yyscan_t)
{
  free (ptr);
}

namespace octave
{
  bool text_parser_tex::init_lexer (const std::string& s)
  {
    if (! scanner)
      octave_tex_lex_init (&scanner);

    if (scanner)
      {
        if (buffer_state)
          {
            octave_tex__delete_buffer (reinterpret_cast<YY_BUFFER_STATE> (buffer_state),
                                       scanner);
            buffer_state = nullptr;
          }

        buffer_state = octave_tex__scan_bytes (s.data (), s.length (), scanner);
      }

    return (scanner && buffer_state);
  }

  void text_parser_tex::destroy_lexer (void)
  {
    if (buffer_state)
      {
        octave_tex__delete_buffer (reinterpret_cast<YY_BUFFER_STATE> (buffer_state),
                                   scanner);
        buffer_state = nullptr;
      }

    if (scanner)
      {
        octave_tex_lex_destroy (scanner);
        scanner = nullptr;
      }
  }
}
