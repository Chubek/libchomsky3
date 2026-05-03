# libchomsky3: A Library for Compiling Regular Expressions to C and Bytecode

Using libchomsky3, you'll have the power of tools such as Re2C and Flex at your disposal. libchomsky3 compiles an ERE-compliant regular expression into imperative C code. It could, alternatively, compile regular expressions to bytecode, for ERE-VM. This virtual machine is a just-in-time compiled VM with low-level ISA, so it does not have 'regex-specific' instructions. This helps a lot with the JIT. Furthermore, ERE-VM bytecode could be AOT'd, and serialized into a structured binary format, ERE-Bincode. These binary files can later be deserialized.


