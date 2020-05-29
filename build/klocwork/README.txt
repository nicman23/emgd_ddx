How to Run

IEGD-KOHEO-LIN
1. export EGD_ROOT variable to the top-root EGD codebase (koheo)
2. make -f $EGD_ROOT/build/klocwork/makefile.klocwork <target> KW_PROJECT=IEGD_KOHEO_HEAD_LIN KW_BUILDCMD="make -f Makefile build_all" 

<target>
:klocwork
- run all three KW step (kw-inject, kw-build, kw-load)
:kw-inject
- generate buildspec
:kw-build
- build using the buildspec generated
:kw-load
- upload the result to server for review