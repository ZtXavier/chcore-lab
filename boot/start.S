#include <common/asm.h>

.extern arm64_elX_to_el1
.extern boot_cpu_stack
.extern secondary_boot_flag
.extern clear_bss_flag
.extern init_c

BEGIN_FUNC(_start)
	mrs	x8, mpidr_el1    // mpidr_el1 中记录PE了cpuid
	and	x8, x8,	#0xFF	  // 这里保留了低八位
	cbz	x8, primary       // 若为零，跳转到primary

  /* hang all secondary processors before we intorduce multi-processors */
secondary_hang:
	bl secondary_hang       								// 若不为零，则挂起

primary:

	/* Turn to el1 from other exception levels. */
	bl 	arm64_elX_to_el1										 //将异常的级别设置为内核态

	/* Prepare stack pointer and jump to C. */
	adr 	x0, boot_cpu_stack
	add 	x0, x0, #0x1000
	mov 	sp, x0

	bl 	init_c

	/* Should never be here */
	b	.
END_FUNC(_start)
