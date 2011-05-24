
project(EigenLapack CXX)

if( NOT DEFINED EIGEN_Fortran_COMPILER_WORKS OR EIGEN_Fortran_COMPILER_WORKS)

  enable_language(Fortran OPTIONAL)

  if(CMAKE_Fortran_COMPILER_WORKS)
    set(EIGEN_Fortran_COMPILER_WORKS TRUE CACHE INTERNAL "workaround cmake's enable_language issue")
  else()
    set(EIGEN_Fortran_COMPILER_WORKS FALSE CACHE INTERNAL "workaround cmake's enable_language issue")
  endif()
  
endif()

if(CMAKE_Fortran_COMPILER_WORKS)

add_custom_target(lapack)
include_directories(../blas)

set(EigenLapack_SRCS
single.cpp double.cpp complex_single.cpp complex_double.cpp
)

get_filename_component(eigen_full_path_to_reference_to_reference_lapack "./reference/" ABSOLUTE)
if(EXISTS ${eigen_full_path_to_reference_to_reference_lapack})
set(EigenLapack_SRCS ${EigenLapack_SRCS}
# reference/dpotrf.f reference/zpotrf.f reference/cpotrf.f reference/spotrf.f
# reference/dpotrs.f reference/spotrs.f reference/zpotrs.f reference/cpotrs.f
# reference/dgetrf.f reference/cgetrf.f reference/sgetrf.f reference/zgetrf.f
# reference/cgetrs.f reference/dgetrs.f reference/sgetrs.f reference/zgetrs.f
# reference/dsyev.f  reference/ssyev.f

reference/dlamch.f  reference/ilaver.f  reference/lsame.f  reference/slamch.f  reference/second_NONE.f  reference/dsecnd_NONE.f
reference/cbdsqr.f               reference/ctbrfs.f               reference/dorml2.f               reference/sla_porfsx_extended.f  reference/zggglm.f
reference/cgbbrd.f               reference/ctbtrs.f               reference/dormlq.f               reference/sla_porpvgrw.f         reference/zgghrd.f
reference/cgbcon.f               reference/ctfsm.f                reference/dormql.f               reference/slapy2.f               reference/zgglse.f
reference/cgbequb.f              reference/ctftri.f               reference/dormqr.f               reference/slapy3.f               reference/zggqrf.f
reference/cgbequ.f               reference/ctfttp.f               reference/dormr2.f               reference/slaqgb.f               reference/zggrqf.f
reference/cgbrfs.f               reference/ctfttr.f               reference/dormr3.f               reference/slaqge.f               reference/zggsvd.f
reference/cgbrfsx.f              reference/ctgevc.f               reference/dormrq.f               reference/slaqp2.f               reference/zggsvp.f
reference/cgbsv.f                reference/ctgex2.f               reference/dormrz.f               reference/slaqps.f               reference/zgtcon.f
reference/cgbsvx.f               reference/ctgexc.f               reference/dormtr.f               reference/slaqr0.f               reference/zgtrfs.f
reference/cgbsvxx.f              reference/ctgsen.f               reference/dpbcon.f               reference/slaqr1.f               reference/zgtsv.f
reference/cgbtf2.f               reference/ctgsja.f               reference/dpbequ.f               reference/slaqr2.f               reference/zgtsvx.f
reference/cgbtrf.f               reference/ctgsna.f               reference/dpbrfs.f               reference/slaqr3.f               reference/zgttrf.f
reference/cgbtrs.f               reference/ctgsy2.f               reference/dpbstf.f               reference/slaqr4.f               reference/zgttrs.f
reference/cgebak.f               reference/ctgsyl.f               reference/dpbsv.f                reference/slaqr5.f               reference/zgtts2.f
reference/cgebal.f               reference/ctpcon.f               reference/dpbsvx.f               reference/slaqsb.f               reference/zhbevd.f
reference/cgebd2.f               reference/ctprfs.f               reference/dpbtf2.f               reference/slaqsp.f               reference/zhbev.f
reference/cgebrd.f               reference/ctptri.f               reference/dpbtrf.f               reference/slaqsy.f               reference/zhbevx.f
reference/cgecon.f               reference/ctptrs.f               reference/dpbtrs.f               reference/slaqtr.f               reference/zhbgst.f
reference/cgeequb.f              reference/ctpttf.f               reference/dpftrf.f               reference/slar1v.f               reference/zhbgvd.f
reference/cgeequ.f               reference/ctpttr.f               reference/dpftri.f               reference/slar2v.f               reference/zhbgv.f
reference/cgees.f                reference/ctrcon.f               reference/dpftrs.f               reference/slarfb.f               reference/zhbgvx.f
reference/cgeesx.f               reference/ctrevc.f               reference/dpocon.f               reference/slarf.f                reference/zhbtrd.f
reference/cgeev.f                reference/ctrexc.f               reference/dpoequb.f              reference/slarfg.f               reference/zhecon.f
reference/cgeevx.f               reference/ctrrfs.f               reference/dpoequ.f               reference/slarfp.f               reference/zheequb.f
reference/cgegs.f                reference/ctrsen.f               reference/dporfs.f               reference/slarft.f               reference/zheevd.f
reference/cgegv.f                reference/ctrsna.f               reference/dporfsx.f              reference/slarfx.f               reference/zheev.f
reference/cgehd2.f               reference/ctrsyl.f               reference/dposv.f                reference/slargv.f               reference/zheevr.f
reference/cgehrd.f               reference/ctrti2.f               reference/dposvx.f               reference/slarnv.f               reference/zheevx.f
reference/cgelq2.f               reference/ctrtri.f               reference/dposvxx.f              reference/sla_rpvgrw.f           reference/zhegs2.f
reference/cgelqf.f               reference/ctrtrs.f               reference/dpotf2.f               reference/slarra.f               reference/zhegst.f
reference/cgelsd.f               reference/ctrttf.f
reference/slarrb.f               reference/zhegvd.f
reference/cgels.f                reference/ctrttp.f               reference/dpotri.f               reference/slarrc.f               reference/zhegv.f
reference/cgelss.f               reference/ctzrqf.f                              reference/slarrd.f               reference/zhegvx.f
reference/cgelsx.f               reference/ctzrzf.f               reference/dppcon.f               reference/slarre.f               reference/zherfs.f
reference/cgelsy.f               reference/cung2l.f               reference/dppequ.f               reference/slarrf.f               reference/zherfsx.f
reference/cgeql2.f               reference/cung2r.f               reference/dpprfs.f               reference/slarrj.f               reference/zhesv.f
reference/cgeqlf.f               reference/cungbr.f               reference/dppsv.f                reference/slarrk.f               reference/zhesvx.f
reference/cgeqp3.f               reference/cunghr.f               reference/dppsvx.f               reference/slarrr.f               reference/zhesvxx.f
reference/cgeqpf.f               reference/cungl2.f               reference/dpptrf.f               reference/slarrv.f               reference/zhetd2.f
reference/cgeqr2.f               reference/cunglq.f               reference/dpptri.f               reference/slarscl2.f             reference/zhetf2.f
reference/cgeqrf.f               reference/cungql.f               reference/dpptrs.f               reference/slartg.f               reference/zhetrd.f
reference/cgerfs.f               reference/cungqr.f               reference/dpstf2.f               reference/slartv.f               reference/zhetrf.f
reference/cgerfsx.f              reference/cungr2.f               reference/dpstrf.f               reference/slaruv.f               reference/zhetri.f
reference/cgerq2.f               reference/cungrq.f               reference/dptcon.f               reference/slarzb.f               reference/zhetrs.f
reference/cgerqf.f               reference/cungtr.f               reference/dpteqr.f               reference/slarz.f                reference/zhfrk.f
reference/cgesc2.f               reference/cunm2l.f               reference/dptrfs.f               reference/slarzt.f               reference/zhgeqz.f
reference/cgesdd.f               reference/cunm2r.f               reference/dptsv.f                reference/slas2.f                reference/zhpcon.f
reference/cgesvd.f               reference/cunmbr.f               reference/dptsvx.f               reference/slascl2.f              reference/zhpevd.f
reference/cgesv.f                reference/cunmhr.f               reference/dpttrf.f               reference/slascl.f               reference/zhpev.f
reference/cgesvx.f               reference/cunml2.f               reference/dpttrs.f               reference/slasd0.f               reference/zhpevx.f
reference/cgesvxx.f              reference/cunmlq.f               reference/dptts2.f               reference/slasd1.f               reference/zhpgst.f
reference/cgetc2.f               reference/cunmql.f               reference/drscl.f                reference/slasd2.f               reference/zhpgvd.f
reference/cgetf2.f               reference/cunmqr.f               reference/dsbevd.f               reference/slasd3.f               reference/zhpgv.f
reference/cunmr2.f               reference/dsbev.f                reference/slasd4.f               reference/zhpgvx.f
reference/cgetri.f               reference/cunmr3.f               reference/dsbevx.f               reference/slasd5.f               reference/zhprfs.f
               reference/cunmrq.f               reference/dsbgst.f               reference/slasd6.f               reference/zhpsv.f
reference/cggbak.f               reference/cunmrz.f               reference/dsbgvd.f               reference/slasd7.f               reference/zhpsvx.f
reference/cggbal.f               reference/cunmtr.f               reference/dsbgv.f                reference/slasd8.f               reference/zhptrd.f
reference/cgges.f                reference/cupgtr.f               reference/dsbgvx.f               reference/slasda.f               reference/zhptrf.f
reference/cggesx.f               reference/cupmtr.f               reference/dsbtrd.f               reference/slasdq.f               reference/zhptri.f
reference/cggev.f                reference/dbdsdc.f               reference/dsfrk.f                reference/slasdt.f               reference/zhptrs.f
reference/cggevx.f               reference/dbdsqr.f               reference/dsgesv.f               reference/slaset.f               reference/zhsein.f
reference/cggglm.f               reference/ddisna.f               reference/dspcon.f               reference/slasq1.f               reference/zhseqr.f
reference/cgghrd.f               reference/dgbbrd.f               reference/dspevd.f               reference/slasq2.f               reference/zlabrd.f
reference/cgglse.f               reference/dgbcon.f               reference/dspev.f                reference/slasq3.f               reference/zlacgv.f
reference/cggqrf.f               reference/dgbequb.f              reference/dspevx.f               reference/slasq4.f               reference/zlacn2.f
reference/cggrqf.f               reference/dgbequ.f               reference/dspgst.f               reference/slasq5.f               reference/zlacon.f
reference/cggsvd.f               reference/dgbrfs.f               reference/dspgvd.f               reference/slasq6.f               reference/zlacp2.f
reference/cggsvp.f               reference/dgbrfsx.f              reference/dspgv.f                reference/slasr.f                reference/zlacpy.f
reference/cgtcon.f               reference/dgbsv.f                reference/dspgvx.f               reference/slasrt.f               reference/zlacrm.f
reference/cgtrfs.f               reference/dgbsvx.f               reference/dsposv.f               reference/slassq.f               reference/zlacrt.f
reference/cgtsv.f                reference/dgbsvxx.f              reference/dsprfs.f               reference/slasv2.f               reference/zladiv.f
reference/cgtsvx.f               reference/dgbtf2.f               reference/dspsv.f                reference/slaswp.f               reference/zlaed0.f
reference/cgttrf.f               reference/dgbtrf.f               reference/dspsvx.f               reference/slasy2.f               reference/zlaed7.f
reference/cgttrs.f               reference/dgbtrs.f               reference/dsptrd.f               reference/sla_syamv.f            reference/zlaed8.f
reference/cgtts2.f               reference/dgebak.f               reference/dsptrf.f               reference/slasyf.f               reference/zlaein.f
reference/chbevd.f               reference/dgebal.f               reference/dsptri.f               reference/sla_syrcond.f          reference/zlaesy.f
reference/chbev.f                reference/dgebd2.f               reference/dsptrs.f               reference/sla_syrfsx_extended.f  reference/zlaev2.f
reference/chbevx.f               reference/dgebrd.f               reference/dstebz.f               reference/sla_syrpvgrw.f         reference/zlag2c.f
reference/chbgst.f               reference/dgecon.f               reference/dstedc.f               reference/slatbs.f               reference/zla_gbamv.f
reference/chbgvd.f               reference/dgeequb.f              reference/dstegr.f               reference/slatdf.f               reference/zla_gbrcond_c.f
reference/chbgv.f                reference/dgeequ.f               reference/dstein.f               reference/slatps.f               reference/zla_gbrcond_x.f
reference/chbgvx.f               reference/dgees.f                reference/dstemr.f               reference/slatrd.f               reference/zla_gbrfsx_extended.f
reference/chbtrd.f               reference/dgeesx.f               reference/dsteqr.f               reference/slatrs.f               reference/zla_gbrpvgrw.f
reference/checon.f               reference/dgeev.f                reference/dsterf.f               reference/slatrz.f               reference/zla_geamv.f
reference/cheequb.f              reference/dgeevx.f               reference/dstevd.f               reference/slatzm.f               reference/zla_gercond_c.f
reference/cheevd.f               reference/dgegs.f                reference/dstev.f                reference/slauu2.f               reference/zla_gercond_x.f
reference/cheev.f                reference/dgegv.f                reference/dstevr.f               reference/slauum.f               reference/zla_gerfsx_extended.f
reference/cheevr.f               reference/dgehd2.f               reference/dstevx.f               reference/sla_wwaddw.f           reference/zlags2.f
reference/cheevx.f               reference/dgehrd.f               reference/dsycon.f               reference/sopgtr.f               reference/zlagtm.f
reference/chegs2.f               reference/dgejsv.f               reference/dsyequb.f              reference/sopmtr.f               reference/zla_heamv.f
reference/chegst.f               reference/dgelq2.f               reference/dsyevd.f               reference/sorg2l.f               reference/zlahef.f
reference/chegvd.f               reference/dgelqf.f               reference/sorg2r.f               reference/zla_hercond_c.f
reference/chegv.f                reference/dgelsd.f               reference/dsyevr.f               reference/sorgbr.f               reference/zla_hercond_x.f
reference/chegvx.f               reference/dgels.f                reference/dsyevx.f               reference/sorghr.f               reference/zla_herfsx_extended.f
reference/cherfs.f               reference/dgelss.f               reference/dsygs2.f               reference/sorgl2.f               reference/zla_herpvgrw.f
reference/cherfsx.f              reference/dgelsx.f               reference/dsygst.f               reference/sorglq.f               reference/zlahqr.f
reference/chesv.f                reference/dgelsy.f               reference/dsygvd.f               reference/sorgql.f               reference/zlahr2.f
reference/chesvx.f               reference/dgeql2.f               reference/dsygv.f                reference/sorgqr.f               reference/zlahrd.f
reference/chesvxx.f              reference/dgeqlf.f               reference/dsygvx.f               reference/sorgr2.f               reference/zlaic1.f
reference/chetd2.f               reference/dgeqp3.f               reference/dsyrfs.f               reference/sorgrq.f               reference/zla_lin_berr.f
reference/chetf2.f               reference/dgeqpf.f               reference/dsyrfsx.f              reference/sorgtr.f               reference/zlals0.f
reference/chetrd.f               reference/dgeqr2.f               reference/dsysv.f                reference/sorm2l.f               reference/zlalsa.f
reference/chetrf.f               reference/dgeqrf.f               reference/dsysvx.f               reference/sorm2r.f               reference/zlalsd.f
reference/chetri.f               reference/dgerfs.f               reference/dsysvxx.f              reference/sormbr.f               reference/zlangb.f
reference/chetrs.f               reference/dgerfsx.f              reference/dsytd2.f               reference/sormhr.f               reference/zlange.f
reference/chfrk.f                reference/dgerq2.f               reference/dsytf2.f               reference/sorml2.f               reference/zlangt.f
reference/chgeqz.f               reference/dgerqf.f               reference/dsytrd.f               reference/sormlq.f               reference/zlanhb.f
reference/chla_transtype.f       reference/dgesc2.f               reference/dsytrf.f               reference/sormql.f               reference/zlanhe.f
reference/chpcon.f               reference/dgesdd.f               reference/dsytri.f               reference/sormqr.f               reference/zlanhf.f
reference/chpevd.f               reference/dgesvd.f               reference/dsytrs.f               reference/sormr2.f               reference/zlanhp.f
reference/chpev.f                reference/dgesv.f                reference/dtbcon.f               reference/sormr3.f               reference/zlanhs.f
reference/chpevx.f               reference/dgesvj.f               reference/dtbrfs.f               reference/sormrq.f               reference/zlanht.f
reference/chpgst.f               reference/dgesvx.f               reference/dtbtrs.f               reference/sormrz.f               reference/zlansb.f
reference/chpgvd.f               reference/dgesvxx.f              reference/dtfsm.f                reference/sormtr.f               reference/zlansp.f
reference/chpgv.f                reference/dgetc2.f               reference/dtftri.f               reference/spbcon.f               reference/zlansy.f
reference/chpgvx.f               reference/dgetf2.f               reference/dtfttp.f               reference/spbequ.f               reference/zlantb.f
reference/chprfs.f
reference/dtfttr.f               reference/spbrfs.f               reference/zlantp.f
reference/chpsv.f                reference/dgetri.f               reference/dtgevc.f               reference/spbstf.f               reference/zlantr.f
reference/chpsvx.f                              reference/dtgex2.f               reference/spbsv.f                reference/zlapll.f
reference/chptrd.f               reference/dggbak.f               reference/dtgexc.f               reference/spbsvx.f               reference/zlapmt.f
reference/chptrf.f               reference/dggbal.f               reference/dtgsen.f               reference/spbtf2.f               reference/zla_porcond_c.f
reference/chptri.f               reference/dgges.f                reference/dtgsja.f               reference/spbtrf.f               reference/zla_porcond_x.f
reference/chptrs.f               reference/dggesx.f               reference/dtgsna.f               reference/spbtrs.f               reference/zla_porfsx_extended.f
reference/chsein.f               reference/dggev.f                reference/dtgsy2.f               reference/spftrf.f               reference/zla_porpvgrw.f
reference/chseqr.f               reference/dggevx.f               reference/dtgsyl.f               reference/spftri.f               reference/zlaqgb.f
reference/clabrd.f               reference/dggglm.f               reference/dtpcon.f               reference/spftrs.f               reference/zlaqge.f
reference/clacgv.f               reference/dgghrd.f               reference/dtprfs.f               reference/spocon.f               reference/zlaqhb.f
reference/clacn2.f               reference/dgglse.f               reference/dtptri.f               reference/spoequb.f              reference/zlaqhe.f
reference/clacon.f               reference/dggqrf.f               reference/dtptrs.f               reference/spoequ.f               reference/zlaqhp.f
reference/clacp2.f               reference/dggrqf.f               reference/dtpttf.f               reference/sporfs.f               reference/zlaqp2.f
reference/clacpy.f               reference/dggsvd.f               reference/dtpttr.f               reference/sporfsx.f              reference/zlaqps.f
reference/clacrm.f               reference/dggsvp.f               reference/dtrcon.f               reference/sposv.f                reference/zlaqr0.f
reference/clacrt.f               reference/dgsvj0.f               reference/dtrevc.f               reference/sposvx.f               reference/zlaqr1.f
reference/cladiv.f               reference/dgsvj1.f               reference/dtrexc.f               reference/sposvxx.f              reference/zlaqr2.f
reference/claed0.f               reference/dgtcon.f               reference/dtrrfs.f               reference/spotf2.f               reference/zlaqr3.f
reference/claed7.f               reference/dgtrfs.f               reference/dtrsen.f
reference/zlaqr4.f
reference/claed8.f               reference/dgtsv.f                reference/dtrsna.f               reference/spotri.f               reference/zlaqr5.f
reference/claein.f               reference/dgtsvx.f               reference/dtrsyl.f                              reference/zlaqsb.f
reference/claesy.f               reference/dgttrf.f               reference/dtrti2.f               reference/sppcon.f               reference/zlaqsp.f
reference/claev2.f               reference/dgttrs.f               reference/dtrtri.f               reference/sppequ.f               reference/zlaqsy.f
reference/clag2z.f               reference/dgtts2.f               reference/dtrtrs.f               reference/spprfs.f               reference/zlar1v.f
reference/cla_gbamv.f            reference/dhgeqz.f               reference/dtrttf.f               reference/sppsv.f                reference/zlar2v.f
reference/cla_gbrcond_c.f        reference/dhsein.f               reference/dtrttp.f               reference/sppsvx.f               reference/zlarcm.f
reference/cla_gbrcond_x.f        reference/dhseqr.f               reference/dtzrqf.f               reference/spptrf.f               reference/zlarfb.f
reference/cla_gbrfsx_extended.f  reference/disnan.f               reference/dtzrzf.f               reference/spptri.f               reference/zlarf.f
reference/cla_gbrpvgrw.f         reference/dlabad.f               reference/dzsum1.f               reference/spptrs.f               reference/zlarfg.f
reference/cla_geamv.f            reference/dlabrd.f               reference/icmax1.f               reference/spstf2.f               reference/zlarfp.f
reference/cla_gercond_c.f        reference/dlacn2.f               reference/ieeeck.f               reference/spstrf.f               reference/zlarft.f
reference/cla_gercond_x.f        reference/dlacon.f               reference/ilaclc.f               reference/sptcon.f               reference/zlarfx.f
reference/cla_gerfsx_extended.f  reference/dlacpy.f               reference/ilaclr.f               reference/spteqr.f               reference/zlargv.f
reference/clags2.f               reference/dladiv.f               reference/iladiag.f              reference/sptrfs.f               reference/zlarnv.f
reference/clagtm.f               reference/dlae2.f                reference/iladlc.f               reference/sptsv.f                reference/zla_rpvgrw.f
reference/cla_heamv.f            reference/dlaebz.f               reference/iladlr.f               reference/sptsvx.f               reference/zlarrv.f
reference/clahef.f               reference/dlaed0.f               reference/ilaenv.f               reference/spttrf.f               reference/zlarscl2.f
reference/cla_hercond_c.f        reference/dlaed1.f               reference/ilaprec.f              reference/spttrs.f               reference/zlartg.f
reference/cla_hercond_x.f        reference/dlaed2.f               reference/ilaslc.f               reference/sptts2.f               reference/zlartv.f
reference/cla_herfsx_extended.f  reference/dlaed3.f               reference/ilaslr.f               reference/srscl.f                reference/zlarzb.f
reference/cla_herpvgrw.f         reference/dlaed4.f               reference/ilatrans.f             reference/ssbevd.f               reference/zlarz.f
reference/clahqr.f               reference/dlaed5.f               reference/ilauplo.f              reference/ssbev.f                reference/zlarzt.f
reference/clahr2.f               reference/dlaed6.f               reference/ilaver.f               reference/ssbevx.f               reference/zlascl2.f
reference/clahrd.f               reference/dlaed7.f               reference/ilazlc.f               reference/ssbgst.f               reference/zlascl.f
reference/claic1.f               reference/dlaed8.f               reference/ilazlr.f               reference/ssbgvd.f               reference/zlaset.f
reference/cla_lin_berr.f         reference/dlaed9.f               reference/iparmq.f               reference/ssbgv.f                reference/zlasr.f
reference/clals0.f               reference/dlaeda.f               reference/izmax1.f               reference/ssbgvx.f               reference/zlassq.f
reference/clalsa.f               reference/dlaein.f               reference/lsamen.f               reference/ssbtrd.f               reference/zlaswp.f
reference/clalsd.f               reference/dlaev2.f               reference/sbdsdc.f               reference/ssfrk.f                reference/zla_syamv.f
reference/clangb.f               reference/dlaexc.f               reference/sbdsqr.f               reference/sspcon.f               reference/zlasyf.f
reference/clange.f               reference/dlag2.f                reference/scsum1.f               reference/sspevd.f               reference/zla_syrcond_c.f
reference/clangt.f               reference/dlag2s.f               reference/sdisna.f               reference/sspev.f                reference/zla_syrcond_x.f
reference/clanhb.f               reference/dla_gbamv.f            reference/sgbbrd.f               reference/sspevx.f               reference/zla_syrfsx_extended.f
reference/clanhe.f               reference/dla_gbrcond.f          reference/sgbcon.f               reference/sspgst.f               reference/zla_syrpvgrw.f
reference/clanhf.f               reference/dla_gbrfsx_extended.f  reference/sgbequb.f              reference/sspgvd.f               reference/zlat2c.f
reference/clanhp.f               reference/dla_gbrpvgrw.f         reference/sgbequ.f               reference/sspgv.f                reference/zlatbs.f
reference/clanhs.f               reference/dla_geamv.f            reference/sgbrfs.f               reference/sspgvx.f               reference/zlatdf.f
reference/clanht.f               reference/dla_gercond.f          reference/sgbrfsx.f              reference/ssprfs.f               reference/zlatps.f
reference/clansb.f               reference/dla_gerfsx_extended.f  reference/sgbsv.f                reference/sspsv.f                reference/zlatrd.f
reference/clansp.f               reference/dlags2.f               reference/sgbsvx.f               reference/sspsvx.f               reference/zlatrs.f
reference/clansy.f               reference/dlagtf.f               reference/sgbsvxx.f              reference/ssptrd.f               reference/zlatrz.f
reference/clantb.f               reference/dlagtm.f               reference/sgbtf2.f               reference/ssptrf.f               reference/zlatzm.f
reference/clantp.f               reference/dlagts.f               reference/sgbtrf.f               reference/ssptri.f               reference/zlauu2.f
reference/clantr.f               reference/dlagv2.f               reference/sgbtrs.f               reference/ssptrs.f               reference/zlauum.f
reference/clapll.f               reference/dlahqr.f               reference/sgebak.f               reference/sstebz.f               reference/zla_wwaddw.f
reference/clapmt.f               reference/dlahr2.f               reference/sgebal.f               reference/sstedc.f               reference/zpbcon.f
reference/cla_porcond_c.f        reference/dlahrd.f               reference/sgebd2.f               reference/sstegr.f               reference/zpbequ.f
reference/cla_porcond_x.f        reference/dlaic1.f               reference/sgebrd.f               reference/sstein.f               reference/zpbrfs.f
reference/cla_porfsx_extended.f  reference/dlaisnan.f             reference/sgecon.f               reference/sstemr.f               reference/zpbstf.f
reference/cla_porpvgrw.f         reference/dla_lin_berr.f         reference/sgeequb.f              reference/ssteqr.f               reference/zpbsv.f
reference/claqgb.f               reference/dlaln2.f               reference/sgeequ.f               reference/ssterf.f               reference/zpbsvx.f
reference/claqge.f               reference/dlals0.f               reference/sgees.f                reference/sstevd.f               reference/zpbtf2.f
reference/claqhb.f               reference/dlalsa.f               reference/sgeesx.f               reference/sstev.f                reference/zpbtrf.f
reference/claqhe.f               reference/dlalsd.f               reference/sgeev.f                reference/sstevr.f               reference/zpbtrs.f
reference/claqhp.f               reference/dlamrg.f               reference/sgeevx.f               reference/sstevx.f               reference/zpftrf.f
reference/claqp2.f               reference/dlaneg.f               reference/sgegs.f                reference/ssycon.f               reference/zpftri.f
reference/claqps.f               reference/dlangb.f               reference/sgegv.f                reference/ssyequb.f              reference/zpftrs.f
reference/claqr0.f               reference/dlange.f               reference/sgehd2.f               reference/ssyevd.f               reference/zpocon.f
reference/claqr1.f               reference/dlangt.f               reference/sgehrd.f               reference/zpoequb.f
reference/claqr2.f               reference/dlanhs.f               reference/sgejsv.f               reference/ssyevr.f               reference/zpoequ.f
reference/claqr3.f               reference/dlansb.f               reference/sgelq2.f               reference/ssyevx.f               reference/zporfs.f
reference/claqr4.f               reference/dlansf.f               reference/sgelqf.f               reference/ssygs2.f               reference/zporfsx.f
reference/claqr5.f               reference/dlansp.f               reference/sgelsd.f               reference/ssygst.f               reference/zposv.f
reference/claqsb.f               reference/dlanst.f               reference/sgels.f                reference/ssygvd.f               reference/zposvx.f
reference/claqsp.f               reference/dlansy.f               reference/sgelss.f               reference/ssygv.f                reference/zposvxx.f
reference/claqsy.f               reference/dlantb.f               reference/sgelsx.f               reference/ssygvx.f               reference/zpotf2.f
reference/clar1v.f               reference/dlantp.f               reference/sgelsy.f               reference/ssyrfs.f
reference/clar2v.f               reference/dlantr.f               reference/sgeql2.f               reference/ssyrfsx.f              reference/zpotri.f
reference/clarcm.f               reference/dlanv2.f               reference/sgeqlf.f               reference/ssysv.f                
reference/clarfb.f               reference/dlapll.f               reference/sgeqp3.f               reference/ssysvx.f               reference/zppcon.f
reference/clarf.f                reference/dlapmt.f               reference/sgeqpf.f               reference/ssysvxx.f              reference/zppequ.f
reference/clarfg.f               reference/dla_porcond.f          reference/sgeqr2.f               reference/ssytd2.f               reference/zpprfs.f
reference/clarfp.f               reference/dla_porfsx_extended.f  reference/sgeqrf.f               reference/ssytf2.f               reference/zppsv.f
reference/clarft.f               reference/dla_porpvgrw.f         reference/sgerfs.f               reference/ssytrd.f               reference/zppsvx.f
reference/clarfx.f               reference/dlapy2.f               reference/sgerfsx.f              reference/ssytrf.f               reference/zpptrf.f
reference/clargv.f               reference/dlapy3.f               reference/sgerq2.f               reference/ssytri.f               reference/zpptri.f
reference/clarnv.f               reference/dlaqgb.f               reference/sgerqf.f               reference/ssytrs.f               reference/zpptrs.f
reference/cla_rpvgrw.f           reference/dlaqge.f               reference/sgesc2.f               reference/stbcon.f               reference/zpstf2.f
reference/clarrv.f               reference/dlaqp2.f               reference/sgesdd.f               reference/stbrfs.f               reference/zpstrf.f
reference/clarscl2.f             reference/dlaqps.f               reference/sgesvd.f               reference/stbtrs.f               reference/zptcon.f
reference/clartg.f               reference/dlaqr0.f               reference/sgesv.f                reference/stfsm.f                reference/zpteqr.f
reference/clartv.f               reference/dlaqr1.f               reference/sgesvj.f               reference/stftri.f               reference/zptrfs.f
reference/clarzb.f               reference/dlaqr2.f               reference/sgesvx.f               reference/stfttp.f               reference/zptsv.f
reference/clarz.f                reference/dlaqr3.f               reference/sgesvxx.f              reference/stfttr.f               reference/zptsvx.f
reference/clarzt.f               reference/dlaqr4.f               reference/sgetc2.f               reference/stgevc.f               reference/zpttrf.f
reference/clascl2.f              reference/dlaqr5.f               reference/sgetf2.f               reference/stgex2.f               reference/zpttrs.f
reference/clascl.f               reference/dlaqsb.f
reference/stgexc.f               reference/zptts2.f
reference/claset.f               reference/dlaqsp.f               reference/sgetri.f               reference/stgsen.f               reference/zrot.f
reference/clasr.f                reference/dlaqsy.f                              reference/stgsja.f               reference/zspcon.f
reference/classq.f               reference/dlaqtr.f               reference/sggbak.f               reference/stgsna.f               reference/zspmv.f
reference/claswp.f               reference/dlar1v.f               reference/sggbal.f               reference/stgsy2.f               reference/zspr.f
reference/cla_syamv.f            reference/dlar2v.f               reference/sgges.f                reference/stgsyl.f               reference/zsprfs.f
reference/clasyf.f               reference/dlarfb.f               reference/sggesx.f               reference/stpcon.f               reference/zspsv.f
reference/cla_syrcond_c.f        reference/dlarf.f                reference/sggev.f                reference/stprfs.f               reference/zspsvx.f
reference/cla_syrcond_x.f        reference/dlarfg.f               reference/sggevx.f               reference/stptri.f               reference/zsptrf.f
reference/cla_syrfsx_extended.f  reference/dlarfp.f               reference/sggglm.f               reference/stptrs.f               reference/zsptri.f
reference/cla_syrpvgrw.f         reference/dlarft.f               reference/sgghrd.f               reference/stpttf.f               reference/zsptrs.f
reference/clatbs.f               reference/dlarfx.f               reference/sgglse.f               reference/stpttr.f               reference/zstedc.f
reference/clatdf.f               reference/dlargv.f               reference/sggqrf.f               reference/strcon.f               reference/zstegr.f
reference/clatps.f               reference/dlarnv.f               reference/sggrqf.f               reference/strevc.f               reference/zstein.f
reference/clatrd.f               reference/dla_rpvgrw.f           reference/sggsvd.f               reference/strexc.f               reference/zstemr.f
reference/clatrs.f               reference/dlarra.f               reference/sggsvp.f               reference/strrfs.f               reference/zsteqr.f
reference/clatrz.f               reference/dlarrb.f               reference/sgsvj0.f               reference/strsen.f               reference/zsycon.f
reference/clatzm.f               reference/dlarrc.f               reference/sgsvj1.f               reference/strsna.f               reference/zsyequb.f
reference/clauu2.f               reference/dlarrd.f               reference/sgtcon.f               reference/strsyl.f               reference/zsymv.f
reference/clauum.f               reference/dlarre.f               reference/sgtrfs.f               reference/strti2.f               reference/zsyr.f
reference/cla_wwaddw.f           reference/dlarrf.f               reference/sgtsv.f                reference/strtri.f               reference/zsyrfs.f
reference/cpbcon.f               reference/dlarrj.f               reference/sgtsvx.f               reference/strtrs.f               reference/zsyrfsx.f
reference/cpbequ.f               reference/dlarrk.f               reference/sgttrf.f               reference/strttf.f               reference/zsysv.f
reference/cpbrfs.f               reference/dlarrr.f               reference/sgttrs.f               reference/strttp.f               reference/zsysvx.f
reference/cpbstf.f               reference/dlarrv.f               reference/sgtts2.f               reference/stzrqf.f               reference/zsysvxx.f
reference/cpbsv.f                reference/dlarscl2.f             reference/shgeqz.f               reference/stzrzf.f               reference/zsytf2.f
reference/cpbsvx.f               reference/dlartg.f               reference/shsein.f               reference/xerbla_array.f         reference/zsytrf.f
reference/cpbtf2.f               reference/dlartv.f               reference/shseqr.f               reference/xerbla.f               reference/zsytri.f
reference/cpbtrf.f               reference/dlaruv.f               reference/sisnan.f               reference/zbdsqr.f               reference/zsytrs.f
reference/cpbtrs.f               reference/dlarzb.f               reference/slabad.f               reference/zcgesv.f               reference/ztbcon.f
reference/cpftrf.f               reference/dlarz.f                reference/slabrd.f               reference/zcposv.f               reference/ztbrfs.f
reference/cpftri.f               reference/dlarzt.f               reference/slacn2.f               reference/zdrscl.f               reference/ztbtrs.f
reference/cpftrs.f               reference/dlas2.f                reference/slacon.f               reference/zgbbrd.f               reference/ztfsm.f
reference/cpocon.f               reference/dlascl2.f              reference/slacpy.f               reference/zgbcon.f               reference/ztftri.f
reference/cpoequb.f              reference/dlascl.f               reference/sladiv.f               reference/zgbequb.f              reference/ztfttp.f
reference/cpoequ.f               reference/dlasd0.f               reference/slae2.f                reference/zgbequ.f               reference/ztfttr.f
reference/cporfs.f               reference/dlasd1.f               reference/slaebz.f               reference/zgbrfs.f               reference/ztgevc.f
reference/cporfsx.f              reference/dlasd2.f               reference/slaed0.f               reference/zgbrfsx.f              reference/ztgex2.f
reference/cposv.f                reference/dlasd3.f               reference/slaed1.f               reference/zgbsv.f                reference/ztgexc.f
reference/cposvx.f               reference/dlasd4.f               reference/slaed2.f               reference/zgbsvx.f               reference/ztgsen.f
reference/cposvxx.f              reference/dlasd5.f               reference/slaed3.f               reference/zgbsvxx.f              reference/ztgsja.f
reference/cpotf2.f               reference/dlasd6.f               reference/slaed4.f               reference/zgbtf2.f               reference/ztgsna.f
reference/dlasd7.f               reference/slaed5.f               reference/zgbtrf.f               reference/ztgsy2.f
reference/cpotri.f               reference/dlasd8.f               reference/slaed6.f               reference/zgbtrs.f               reference/ztgsyl.f
               reference/dlasda.f               reference/slaed7.f               reference/zgebak.f               reference/ztpcon.f
reference/cppcon.f               reference/dlasdq.f               reference/slaed8.f               reference/zgebal.f               reference/ztprfs.f
reference/cppequ.f               reference/dlasdt.f               reference/slaed9.f               reference/zgebd2.f               reference/ztptri.f
reference/cpprfs.f               reference/dlaset.f               reference/slaeda.f               reference/zgebrd.f               reference/ztptrs.f
reference/cppsv.f                reference/dlasq1.f               reference/slaein.f               reference/zgecon.f               reference/ztpttf.f
reference/cppsvx.f               reference/dlasq2.f               reference/slaev2.f               reference/zgeequb.f              reference/ztpttr.f
reference/cpptrf.f               reference/dlasq3.f               reference/slaexc.f               reference/zgeequ.f               reference/ztrcon.f
reference/cpptri.f               reference/dlasq4.f               reference/slag2d.f               reference/zgees.f                reference/ztrevc.f
reference/cpptrs.f               reference/dlasq5.f               reference/slag2.f                reference/zgeesx.f               reference/ztrexc.f
reference/cpstf2.f               reference/dlasq6.f               reference/sla_gbamv.f            reference/zgeev.f                reference/ztrrfs.f
reference/cpstrf.f               reference/dlasr.f                reference/sla_gbrcond.f          reference/zgeevx.f               reference/ztrsen.f
reference/cptcon.f               reference/dlasrt.f               reference/sla_gbrfsx_extended.f  reference/zgegs.f                reference/ztrsna.f
reference/cpteqr.f               reference/dlassq.f               reference/sla_gbrpvgrw.f         reference/zgegv.f                reference/ztrsyl.f
reference/cptrfs.f               reference/dlasv2.f               reference/sla_geamv.f            reference/zgehd2.f               reference/ztrti2.f
reference/cptsv.f                reference/dlaswp.f               reference/sla_gercond.f          reference/zgehrd.f               reference/ztrtri.f
reference/cptsvx.f               reference/dlasy2.f               reference/sla_gerfsx_extended.f  reference/zgelq2.f               reference/ztrtrs.f
reference/cpttrf.f               reference/dla_syamv.f            reference/slags2.f               reference/zgelqf.f               reference/ztrttf.f
reference/cpttrs.f               reference/dlasyf.f               reference/slagtf.f               reference/zgelsd.f               reference/ztrttp.f
reference/cptts2.f               reference/dla_syrcond.f          reference/slagtm.f               reference/zgels.f                reference/ztzrqf.f
reference/crot.f                 reference/dla_syrfsx_extended.f  reference/slagts.f               reference/zgelss.f               reference/ztzrzf.f
reference/cspcon.f               reference/dla_syrpvgrw.f         reference/slagv2.f               reference/zgelsx.f               reference/zung2l.f
reference/cspmv.f                reference/dlat2s.f               reference/slahqr.f               reference/zgelsy.f               reference/zung2r.f
reference/cspr.f                 reference/dlatbs.f               reference/slahr2.f               reference/zgeql2.f               reference/zungbr.f
reference/csprfs.f               reference/dlatdf.f               reference/slahrd.f               reference/zgeqlf.f               reference/zunghr.f
reference/cspsv.f                reference/dlatps.f               reference/slaic1.f               reference/zgeqp3.f               reference/zungl2.f
reference/cspsvx.f               reference/dlatrd.f               reference/slaisnan.f             reference/zgeqpf.f               reference/zunglq.f
reference/csptrf.f               reference/dlatrs.f               reference/sla_lin_berr.f         reference/zgeqr2.f               reference/zungql.f
reference/csptri.f               reference/dlatrz.f               reference/slaln2.f               reference/zgeqrf.f               reference/zungqr.f
reference/csptrs.f               reference/dlatzm.f               reference/slals0.f               reference/zgerfs.f               reference/zungr2.f
reference/csrscl.f               reference/dlauu2.f               reference/slalsa.f               reference/zgerfsx.f              reference/zungrq.f
reference/cstedc.f               reference/dlauum.f               reference/slalsd.f               reference/zgerq2.f               reference/zungtr.f
reference/cstegr.f               reference/dla_wwaddw.f           reference/slamrg.f               reference/zgerqf.f               reference/zunm2l.f
reference/cstein.f               reference/dopgtr.f               reference/slaneg.f               reference/zgesc2.f               reference/zunm2r.f
reference/cstemr.f               reference/dopmtr.f               reference/slangb.f               reference/zgesdd.f               reference/zunmbr.f
reference/csteqr.f               reference/dorg2l.f               reference/slange.f               reference/zgesvd.f               reference/zunmhr.f
reference/csycon.f               reference/dorg2r.f               reference/slangt.f               reference/zgesv.f                reference/zunml2.f
reference/csyequb.f              reference/dorgbr.f               reference/slanhs.f               reference/zgesvx.f               reference/zunmlq.f
reference/csymv.f                reference/dorghr.f               reference/slansb.f               reference/zgesvxx.f              reference/zunmql.f
reference/csyr.f                 reference/dorgl2.f               reference/slansf.f               reference/zgetc2.f               reference/zunmqr.f
reference/csyrfs.f               reference/dorglq.f               reference/slansp.f               reference/zgetf2.f               reference/zunmr2.f
reference/csyrfsx.f              reference/dorgql.f               reference/slanst.f
reference/zunmr3.f
reference/csysv.f                reference/dorgqr.f               reference/slansy.f               reference/zgetri.f               reference/zunmrq.f
reference/csysvx.f               reference/dorgr2.f               reference/slantb.f                              reference/zunmrz.f
reference/csysvxx.f              reference/dorgrq.f               reference/slantp.f               reference/zggbak.f               reference/zunmtr.f
reference/csytf2.f               reference/dorgtr.f               reference/slantr.f               reference/zggbal.f               reference/zupgtr.f
reference/csytrf.f               reference/dorm2l.f               reference/slanv2.f               reference/zgges.f                reference/zupmtr.f
reference/csytri.f               reference/dorm2r.f               reference/slapll.f               reference/zggesx.f
reference/csytrs.f               reference/dormbr.f               reference/slapmt.f               reference/zggev.f
reference/ctbcon.f               reference/dormhr.f               reference/sla_porcond.f          reference/zggevx.f
)
endif()

add_library(eigen_lapack_static ${EigenLapack_SRCS})
add_library(eigen_lapack SHARED ${EigenLapack_SRCS})

if(EIGEN_STANDARD_LIBRARIES_TO_LINK_TO)
  target_link_libraries(eigen_lapack_static ${EIGEN_STANDARD_LIBRARIES_TO_LINK_TO})
  target_link_libraries(eigen_lapack        ${EIGEN_STANDARD_LIBRARIES_TO_LINK_TO})
endif()

# add_dependencies(lapack eigen_lapack eigen_lapack_static)
add_dependencies(lapack eigen_lapack_static)

# install(TARGETS eigen_lapack
#         RUNTIME DESTINATION bin
#         LIBRARY DESTINATION lib
#         ARCHIVE DESTINATION lib)


#   add_subdirectory(testing)
endif(CMAKE_Fortran_COMPILER_WORKS)
