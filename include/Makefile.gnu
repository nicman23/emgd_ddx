#----------------------------------------------------------------------------
# Filename: Makefile.gnu
#----------------------------------------------------------------------------
# Copyright (c) 2002-2013, Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# 
#----------------------------------------------------------------------------
export EGD_TOPLEVEL = X Headers

include ../../../Makefile.include

IEGD_INS ?= $(XROOT)/include/X11/extensions

all::

clean::

install::
	echo -e "$(GREEN)Installing $(IEGD_TOPLEVEL)$(OFF)"
	@echo -ne "$(GREEN)"; \
		echo " Installing $(IEGD_INS)/intel_escape.h"; \
		echo -ne "$(OFF)";
	@install -D -c -m 0444 intel_escape.h $(IEGD_INS)/intel_escape.h
	@echo -ne "$(GREEN)"; \
		echo " Installing $(IEGD_INS)/iegd_escape.h"; \
		echo -ne "$(OFF)";
	@install -D -c -m 0444 ../../include/iegd_escape.h \
		$(IEGD_INS)/iegd_escape.h

package::
	mkdir -p $(IEGD_PKG)/driver/common
	$(MAKE) IEGD_INS=$(IEGD_PKG)/driver/common install
