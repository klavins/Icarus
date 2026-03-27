/* MinGW runtime stubs for freestanding UEFI build */

/* Stack probe — not needed in freestanding, just return */
void __attribute__((naked)) ___chkstk_ms(void) {
    asm volatile ("ret");
}
