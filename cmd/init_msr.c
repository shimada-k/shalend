#include "msr.h"
#include "msr_address.h"

/*
	依存関係 msr.ko bitopsライブラリ cpuidライブラリ
*/

int main(int argc, char *argv[])
{
	int nr_ia32_pmcs;
	union IA32_PERFEVTSELx reg;
	reg.full = 0;

	/* PerfGlobalCtrlレジスタを設定 */
	nr_ia32_pmcs = setup_PERF_GLOBAL_CTRL();

	printf("%d nr_ia32_pmcs registered.\n", nr_ia32_pmcs);

	/* PERFEVENTSELの設定 */

	reg.split.EvtSel = EVENT_MEM_LOAD_RETIRED_MISS;
	reg.split.UMASK = UMASK_MEM_LOAD_RETIRED_MISS;
//	reg.split.EvtSel = EVENT_LONGEST_CACHE_LAT;
//	reg.split.UMASK = UMASK_LONGEST_CACHE_LAT_MISS;

	reg.split.EN = 1;

	reg.split.USER = 1;
	reg.split.OS = 0;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL0, &reg);

	reg.split.USER = 0;
	reg.split.OS = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL1, &reg);

	reg.split.USER = 1;
	reg.split.OS = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL2, &reg);

	//setup_IA32_PERFEVTSEL_quickly(IA32_PERFEVENTSEL0, UMASK_LONGEST_CACHE_LAT_MISS, EVENT_LONGEST_CACHE_LAT);
	//setup_IA32_PERFEVTSEL_quickly(IA32_PERFEVENTSEL1, UMASK_LONGEST_CACHE_LAT_REFERENCE, EVENT_LONGEST_CACHE_LAT);

	return 0;
}
