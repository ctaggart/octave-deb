# Installation paths for use in debian/rules of Octave-related packages
# Written by Rafael Laboissiere <rafael@debian.org>
# $Id$

MDIR = $(shell octave-config-@VERSION@  --print LOCALFCNFILEDIR)
OCTDIR = $(shell octave-config-@VERSION@  --print LOCALAPIOCTFILEDIR)
