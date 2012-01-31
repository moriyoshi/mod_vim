mod_vim.la: mod_vim.slo ga.slo utils.slo conv.slo remote.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version mod_vim.lo ga.lo utils.lo conv.lo remote.lo $(LIBS)
DISTCLEAN_TARGETS = modules.mk
shared =  mod_vim.la
