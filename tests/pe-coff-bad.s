/* Fixture for the "unknown relocation types must fail loudly" requirement.
   ".rva" makes GNU as emit IMAGE_REL_I386_DIR32NB (type 7), an RVA relative
   to the image base.  tcc has no internal relocation with those semantics, so
   the reader must refuse the object by name and number rather than guess. */
	.data
	.globl	pc_badrva
pc_badrva:
	.rva	pc_badrva
