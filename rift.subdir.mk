#
# A version of <bsd.subdir.mk> derived from netbsd.
#
# Allows .WAIT to be specified in $SUBDIRS for a simple dependency system that
# works with parallel builds.
#
# Note that this only works perfectly with bmake's mk-files;
# netbsd's _THISDIR_ has a trailing slash as of 2022-09-27,
# so output would be slightly wrong.
# This variable is only used for pretty-printing the directory name AFAIK.
#

.include <bsd.init.mk>

#
# Derived from netbsd MAKEDIRTARGET as defined in <bsd.own.mk>.
#
# No arguments are required.
# `dir' defaults to `.'.
# `target' defaults to nothing, which would cause the default target to be used.
MAKEDIRTARGET = \
	@_mkdirtarg() { \
		dir="$${1:-.}"; shift; \
		targ="$$1"; shift; \
		case "$$dir" in \
		/*) \
			rel="$$dir"; \
			abs="$$dir"; \
			;; \
		.) \
			rel="$(_THISDIR_)"; \
			abs="$(.CURDIR)"; \
			;; \
		*) \
			rel="$(_THISDIR_)$$dir"; \
			abs="$(.CURDIR)/$$dir"; \
			;; \
		esac; \
		echo "===> $${rel} [$$targ]$${1:+ (with $$@)}"; \
		cd "$$abs" \
		&& $(MAKE) _THISDIR_="$$rel" "$$@" $$targ; \
	}; \
	_mkdirtarg

#
# The following conceptually just generates the appropriate targets for
# descending into subdirs.
#
# Generate a variable SUBDIR_targ for each target given on the command line
# (or the default `all').
# It contains a list of targets of the form dir,target.
# Each of these will run MAKEDIRTARGET appropriately.
#

.for targ in $(.TARGETS) # {

.for dir in $(SUBDIR)
.if $(dir) == ".WAIT"
SUBDIR_$(targ) += .WAIT
.else
# create a target specifically for this dir,target combination,
# and add it to the list
$(dir),$(targ): .PHONY .MAKE
	@$(MAKEDIRTARGET) $(.TARGET:C/,[^,]*$//) $(.TARGET:C/.*,//)
SUBDIR_$(targ) += $(dir),$(targ)
.endif
.endfor

# notably skip the creation of intermediate `subdir-$(targ)' target
$(targ): .PHONY $(SUBDIR_$(targ))

.endfor # }
