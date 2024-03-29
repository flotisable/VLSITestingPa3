srcDir        := src
binDir        := bin
testDir       := Test
cktDir        := sample_circuits
patternDir    := tdf_patterns
PROG          := atpg
goldenProgram := golden_tdfsim

circuits := $(patsubst ${cktDir}/%.ckt, %, $(wildcard ${cktDir}/*.ckt) )

reportProgram := reportStatistic.perl

include settings

.PHONY: all ${srcDir}/${PROG} clean test tags report tar scripts installScripts

all: ${srcDir}/${PROG}

${srcDir}/${PROG}:
	${MAKE} -C ${srcDir}

test: ${testDir} ${srcDir}/${PROG}
	@for circuit in ${circuits}; do \
		echo "test $${circuit}"; \
		circuitFull=${cktDir}/$${circuit}.ckt; \
		pattern=${patternDir}/$${circuit}.pat; \
		goldenLog=${testDir}/golden_$${circuit}.log; \
		log=${testDir}/$${circuit}.log; \
		./${srcDir}/${PROG} -tdfsim $${pattern} $${circuitFull} >& $${log}; \
		diff -y --suppress-common-lines $${goldenLog} $${log}; \
		if [ "$$?" != "0" ]; then \
			echo "$${circuit} is not the same as golden result!"; \
		fi \
	done

${testDir}:
	mkdir $@

tags:
	ctags ${srcDir}/*.{h,cpp}

report: settings
	@for circuit in ${reportCircuits}; do \
		circuitFull=${cktDir}/$${circuit}.ckt; \
		pattern=${patternDir}/$${circuit}.pat; \
		./${srcDir}/${PROG} -tdfsim $${pattern} $${circuitFull} |& ./${reportProgram}; \
	done

tar: settings
	@if [ -d ${archiveName} ]; then \
		rm -r ${archiveName}; \
	fi
	@mkdir ${archiveName}
	cp -r ${archiveFiles} ${archiveName}
	tar -czf ${archiveName}.tgz ${archiveName}

scripts: settings
	@if [ -d $@ ]; then \
		rm -r $@; \
	fi
	@mkdir $@
	cp ${scriptFiles} $@
	tar -czf $@.tgz $@

installScripts: settings
	cp ${scriptFiles} ${scriptInstallPath}

clean: settings
	rm ${srcDir}/*.o ${srcDir}/${PROG} *.tgz tags
	@if [ -d ${archiveName} ]; then \
		rm -r ${archiveName}; \
	fi
	@if [ -d $@ ]; then \
		rm -r $@; \
	fi
