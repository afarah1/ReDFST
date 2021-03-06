#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <cpufreq.h>
#include "macros.h"
#include "region.h"
#include "perf.h"
#include "util.h"
#include "global.h"
#include "libredfst_config.h"
static inline redfst_region_t * region_get(int cpu, int id){
	return &gRedfstRegion[cpu][id];
}

#define id_is_fast(id) (gRedfstFastRegions&(1LL<<(id)))
#define id_is_slow(id) (gRedfstSlowRegions&(1LL<<(id)))
static int redfst_freq_get(redfst_region_t *m, int id){
	if(id_is_fast(id))
		return FREQ_HIGH;
	else if(id_is_slow(id))
		return FREQ_LOW;
	return 0;
}


static void redfst_region_impl(int id, int cpu){
	redfst_region_t *m;
	uint64_t timeNow;
	int freq;
//	if(tRedfstCpu!=0&&tRedfstCpu!=1) return; // might make sense if freq is per cpu
	m = region_get(cpu,tRedfstPrevId);
	++m->next[id];

	// don't waste too many cycles when called inside loops
	if(likely(id == tRedfstPrevId)){
		return;
	}
	tRedfstPrevId = id;
	timeNow = time_now();
	m->time += timeNow - m->timeStarted;
	region_get(cpu,id)->timeStarted = timeNow;

	// for the execution monitor
	gRedfstCurrentId[cpu] = id;

	redfst_perf_read(cpu, &m->perf);

#ifndef REDFSTLIB_FREQ_PER_CORE
	if(REDFSTLIB_CPU0 != cpu && REDFSTLIB_CPU1 != cpu)
		return;
#endif
	freq = redfst_freq_get(region_get(cpu,id), id);
	if(unlikely(freq && freq != gRedfstCurrentFreq[cpu])){
		gRedfstCurrentFreq[cpu] = freq;
		cpufreq_set_frequency(cpu, freq);
	}
}

#ifdef REDFSTLIB_STATIC
static
#endif
void redfst_region(int id){
	redfst_region_impl(id, tRedfstCpu);
}

#ifdef REDFSTLIB_STATIC
static
#endif
void redfst_region_final(){
	int cpu;
	for(cpu=0; cpu < gRedfstThreadCount;++cpu){
		redfst_region_impl(REDFSTLIB_MAX_REGIONS-1, cpu);
	}
}

#include <time.h>
#ifdef REDFSTLIB_STATIC
static
#endif
void redfst_impl(const char *cmd){
	struct timespec spec;
	clock_gettime(CLOCK_MONOTONIC, &spec);
	printf("%lld.%.9ld ENERGY %s\n",(long long)spec.tv_sec,spec.tv_nsec,cmd);
	fflush(stdout);
}
#ifdef REDFSTLIB_STATIC
static
#endif
void redfst_reset(){ redfst_impl("reset"); }
#ifdef REDFSTLIB_STATIC
static
#endif
void redfst_print(){ redfst_impl("print"); }
#ifdef REDFSTLIB_STATIC
static
#endif
void redfst_exit() { redfst_impl("exit" ); }


#ifdef REDFSTLIB_OMP
#include <omp.h>
#ifdef REDFSTLIB_STATIC
static
#endif
void redfst_region_all(int id){
	int nthreads;
	int i;
	nthreads = omp_get_max_threads();
#pragma omp parallel for num_threads(nthreads)
	for(i=0; i < nthreads; ++i)
		redfst_region(id);
}
#endif
