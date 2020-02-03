>Exercise 9. In your kern/trap.c, call kbd_intr to handle trap IRQ_OFFSET+IRQ_KBD and serial_intr to handle trap IRQ_OFFSET+IRQ_SERIAL.

Test your code by running make run-testkbd and type a few lines. The system should echo your lines back to you as you finish them. Try typing in both the console and the graphical window, if you have both available.

# 代码

```
static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
		lapic_eoi();
		kbd_intr();
		return;
	}
	else if(tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
		lapic_eoi();
		serial_intr();
		return;
	}

}
```