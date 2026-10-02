#include "lunu_primitives.h"
#include <stdio.h>
#include <time.h>

int main(void) {
    const unsigned rounds = 1000000u; lunu_path p = {0}, q; lunu_state s = {0};
    clock_t start = clock();
    for (unsigned i=0;i<rounds;i++) { if (p.depth == 256) p.depth = 0; (void)lunu_child(&p, (i&1u)!=0, &p); }
    printf("child: %.3f ns/op\n", 1e9*(double)(clock()-start)/CLOCKS_PER_SEC/rounds);
    start=clock(); for(unsigned i=0;i<rounds;i++) { (void)lunu_parent(&p,&q); (void)lunu_prefix(&p,p.depth/2u,&q); (void)lunu_neighbor(&p,p.depth?i%p.depth:0,&q); }
    printf("parent+prefix+neighbor: %.3f ns/op each\n", 1e9*(double)(clock()-start)/CLOCKS_PER_SEC/(3.0*rounds));
    unsigned char data[32]={0}; start=clock(); for(unsigned i=0;i<rounds;i++) { if(s.path.depth==256) s=(lunu_state){0}; (void)lunu_step_payload(&s,(i&1u)!=0,data,256,LUNU_CONTROL_ROLLING_HASH); }
    printf("step rolling 256-bit payload: %.3f ns/op\n", 1e9*(double)(clock()-start)/CLOCKS_PER_SEC/rounds);
    printf("sizeof(path)=%zu sizeof(state)=%zu\n",sizeof(lunu_path),sizeof(lunu_state)); return 0;
}
