## Copyright (C) 2013-2019 Vytautas Jančauskas
##
## This file is part of Octave.
##
## Octave is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## Octave is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Octave; see the file COPYING.  If not, see
## <https://www.gnu.org/licenses/>.

## -*- texinfo -*-
## @deftypefn {} {} stop (@var{player})
## Stop the playback for the audioplayer @var{player} and reset the
## relevant variables to their starting values.
## @end deftypefn

function stop (player)

  if (nargin != 1)
    print_usage ();
  endif

  __player_stop__ (struct (player).player);

endfunction
