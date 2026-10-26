/* The module loader patches PAC into SCS only from .init.eh_frame.
 * Keep original FDEs and include the newly compiled C/assembly FDEs.
 * No exported symbol, CRC, vermagic or module layout is changed here.
 */
SECTIONS {
 .init.eh_frame : { *(.init.eh_frame) *(.eh_frame) }
}
