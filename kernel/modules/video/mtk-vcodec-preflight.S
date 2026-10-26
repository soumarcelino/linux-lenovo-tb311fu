/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Exact 6.6 vendor module preflight. The original probe starts with PACIASP.
 * Its first instruction branches here; success rejoins at probe+4 with LR
 * signed at the original SP. Failure authenticates LR and returns before any
 * video node, workqueue, IRQ or devm allocation is created.
 *
 * The generator patches the final B to probe+4 after relocatable linking.
 * iommu_get_domain_for_dev is already an import in both original modules.
 * CFI negate_ra_state metadata is mandatory: this kernel dynamically turns
 * PACIASP/AUTIASP into SCS push/pop using .init.eh_frame, including the
 * original epilogues reached through our branches. Never omit these records.
 */
.text
.balign 4
.global tb311fu_vcodec_preflight
.type tb311fu_vcodec_preflight, %function
tb311fu_vcodec_preflight:
	.cfi_startproc
	paciasp
	.cfi_negate_ra_state
	stp x29, x30, [sp, #-32]!
	.cfi_def_cfa_offset 32
	.cfi_offset x29, -32
	.cfi_offset x30, -24
	str x0, [sp, #16]
	/* struct platform_device.dev offset, verified against the vendor probe. */
	add x0, x0, #16
	bl iommu_get_domain_for_dev
	cbz x0, .Ldefer
	ldr x0, [sp, #16]
	ldp x29, x30, [sp], #32
	.cfi_def_cfa_offset 0
.global tb311fu_vcodec_rejoin
tb311fu_vcodec_rejoin:
	b .
.Ldefer:
	.cfi_def_cfa_offset 32
	mov w0, #-517 /* -EPROBE_DEFER */
	ldp x29, x30, [sp], #32
	.cfi_def_cfa_offset 0
	autiasp
	.cfi_negate_ra_state
	ret
	.cfi_endproc
.size tb311fu_vcodec_preflight, .-tb311fu_vcodec_preflight

/* Direct call from the configured-kernel C guard into the original open body.
 * The original PACIASP was replaced by a branch to the guard; reproduce it
 * for this call before joining open+4. No indirect-call CFI is bypassed.
 */
.balign 4
.global tb311fu_vcodec_call_open
.type tb311fu_vcodec_call_open, %function
tb311fu_vcodec_call_open:
	.cfi_startproc
	paciasp
	.cfi_negate_ra_state
.global tb311fu_vcodec_open_rejoin
tb311fu_vcodec_open_rejoin:
	b .
	.cfi_endproc
.size tb311fu_vcodec_call_open, .-tb311fu_vcodec_call_open
.balign 4
.global tb311fu_vcodec_call_remove
.type tb311fu_vcodec_call_remove, %function
tb311fu_vcodec_call_remove:
 .cfi_startproc
 paciasp
 .cfi_negate_ra_state
.global tb311fu_vcodec_remove_rejoin
tb311fu_vcodec_remove_rejoin:
 b .
 .cfi_endproc
.size tb311fu_vcodec_call_remove, .-tb311fu_vcodec_call_remove
.section .note.GNU-stack,"",%progbits
