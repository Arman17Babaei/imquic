ACLOCAL_AMFLAGS = -I m4

# FIXME: make docs work with distcheck
DISTCHECK_CONFIGURE_FLAGS = --disable-docs

EXTRA_DIST = \
	imquic.pc.in \
	l4s/mininet-benchmark.md \
	l4s/qemu-evidence/mininet-benchmark-analysis.json \
	l4s/qemu-evidence/mininet-benchmark-environment.json \
	l4s/qemu-evidence/mininet-benchmark-summary.csv \
	tools/l4s/analyze_mininet_benchmark.py \
	tools/l4s/analyze_timeseries.py \
	tools/l4s/run_mininet_benchmark.py \
	tools/l4s/run_qemu_timeseries_test.py \
	tools/l4s/run_timeseries_test.sh
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = imquic.pc
DISTCLEANFILES = imquic.pc

SUBDIRS = src examples docs
#~ dist_html_DATA = README.md

.PHONY: FORCE
FORCE:

.PHONY: l4s-timeseries-check
l4s-timeseries-check:
	python3 $(top_srcdir)/tools/l4s/run_qemu_timeseries_test.py

.PHONY: l4s-timeseries-guest-check
l4s-timeseries-guest-check:
	$(top_srcdir)/tools/l4s/run_timeseries_test.sh $(L4S_RESULT_DIR)

L4S_BENCHMARK_RATES ?= 0,5,10,20
L4S_BENCHMARK_BYTES ?= 4194304
L4S_BENCHMARK_BOTTLENECK ?= 20mbit
L4S_BENCHMARK_SECONDS ?= 8
L4S_BENCHMARK_REPETITIONS ?= 5
L4S_RESULT_DIR ?= $(top_builddir)/results/l4s/manual-run

.PHONY: l4s-mininet-benchmark-check
l4s-mininet-benchmark-check:
	python3 $(top_srcdir)/tools/l4s/run_qemu_timeseries_test.py \
		--make-target l4s-mininet-benchmark-guest-check \
		--guest-result-name mininet-benchmark \
		--destination-prefix qemu-mininet-benchmark \
		--make-variable L4S_BENCHMARK_RATES=$(L4S_BENCHMARK_RATES) \
		--make-variable L4S_BENCHMARK_BYTES=$(L4S_BENCHMARK_BYTES) \
		--make-variable L4S_BENCHMARK_BOTTLENECK=$(L4S_BENCHMARK_BOTTLENECK) \
		--make-variable L4S_BENCHMARK_SECONDS=$(L4S_BENCHMARK_SECONDS) \
		--make-variable L4S_BENCHMARK_REPETITIONS=$(L4S_BENCHMARK_REPETITIONS)

.PHONY: l4s-mininet-benchmark-guest-check
l4s-mininet-benchmark-guest-check:
	python3 $(top_srcdir)/tools/l4s/run_mininet_benchmark.py \
		--output $(L4S_RESULT_DIR) \
		--background-mbps $(L4S_BENCHMARK_RATES) \
		--transfer-bytes $(L4S_BENCHMARK_BYTES) \
		--bottleneck $(L4S_BENCHMARK_BOTTLENECK) \
		--background-seconds $(L4S_BENCHMARK_SECONDS) \
		--repetitions $(L4S_BENCHMARK_REPETITIONS)
