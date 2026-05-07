/**
 * @file Chomsky3.i
 * @brief Complete documentation for the libchomsky3 regular expression engine
 * 
 * @mainpage Chomsky3 Regular Expression Library
 * 
 * @section intro_sec Introduction
 * 
 * Chomsky3 is a high-performance regular expression engine designed for
 * production use in systems requiring predictable performance, resource
 * control, and security guarantees.
 * 
 * The library provides:
 * - **Bytecode compilation** from regex patterns to portable instruction format
 * - **Interpreter-based execution** with backtracking and capture groups
 * - **JIT compilation** via SLJIT for native code generation
 * - **Resource limits** to prevent ReDoS (Regular Expression Denial of Service)
 * - **Multiple execution modes** for debugging, profiling, and tracing
 * - **Thread-safe design** with no global state
 * 
 * @section arch_sec Architecture Overview
 * 
 * @subsection arch_pipeline Execution Pipeline
 * 
 * 1. **Pattern Parsing** → Abstract Syntax Tree (AST)
 * 2. **AST Compilation** → Bytecode instructions
 * 3. **Bytecode Optimization** → Peephole and control flow optimization
 * 4. **Execution** → Interpreter or JIT-compiled native code
 * 5. **Result Extraction** → Match positions and capture groups
 * 
 * @subsection arch_components Core Components
 * 
 * - **Parser** (`src/parser/`): Converts regex strings to AST
 * - **Compiler** (`src/compiler/`): Transforms AST to bytecode
 * - **Bytecode** (`src/bytecode/`): Instruction format and serialization
 * - **VM** (`src/vm/`): Interpreter and execution engine
 * - **JIT** (`src/jit/`): Native code generation via SLJIT
 * - **Optimizer** (`src/optimizer/`): Bytecode optimization passes
 * 
 * @section bytecode_sec Bytecode Format
 * 
 * @subsection bytecode_header Bytecode Header
 * 
 * Every compiled bytecode begins with a header:
 * 
 * @code{.c}
 * typedef struct {
 *     uint32_t magic;                  // CHOMSKY3_BYTECODE_MAGIC (0x43484F33)
 *     uint32_t version;                // Format version
 *     uint32_t flags;                  // Compilation flags
 *     uint32_t instruction_count;      // Number of instructions
 *     uint32_t data_size;              // Size of data section
 *     uint32_t capture_count;          // Number of capture groups
 *     uint32_t checksum;               // CRC32 checksum
 * } chomsky3_bytecode_header_t;
 * @endcode
 * 
 * @subsection bytecode_instruction Instruction Format
 * 
 * Each instruction is 16 bytes:
 * 
 * @code{.c}
 * typedef struct {
 *     uint8_t  opcode;      // Operation code (0-255)
 *     uint8_t  flags;       // Instruction flags
 *     uint16_t reserved;    // Reserved for alignment
 *     uint32_t operand1;    // First operand
 *     uint32_t operand2;    // Second operand
 *     uint32_t operand3;    // Third operand
 * } chomsky3_instruction_t;
 * @endcode
 * 
 * @subsection bytecode_opcodes Opcode Categories
 * 
 * **Character Matching (0x00-0x0F)**
 * - `CHOMSKY3_OP_CHAR` (0x00): Match single character
 * - `CHOMSKY3_OP_CHAR_RANGE` (0x01): Match character range [a-z]
 * - `CHOMSKY3_OP_ANY_CHAR` (0x02): Match any character (.)
 * - `CHOMSKY3_OP_CHAR_CLASS` (0x03): Match character class [abc]
 * - `CHOMSKY3_OP_CHAR_CLASS_NEG` (0x04): Negated class [^abc]
 * - `CHOMSKY3_OP_DIGIT` (0x05): Match digit \d
 * - `CHOMSKY3_OP_NON_DIGIT` (0x06): Match non-digit \D
 * - `CHOMSKY3_OP_WORD` (0x07): Match word character \w
 * - `CHOMSKY3_OP_NON_WORD` (0x08): Match non-word \W
 * - `CHOMSKY3_OP_WHITESPACE` (0x09): Match whitespace \s
 * - `CHOMSKY3_OP_NON_WHITESPACE` (0x0A): Match non-whitespace \S
 * 
 * **String Matching (0x10-0x1F)**
 * - `CHOMSKY3_OP_STRING` (0x10): Match literal string
 * - `CHOMSKY3_OP_STRING_ICASE` (0x11): Case-insensitive string match
 * 
 * **Control Flow (0x20-0x2F)**
 * - `CHOMSKY3_OP_JUMP` (0x20): Unconditional jump
 * - `CHOMSKY3_OP_SPLIT` (0x21): Fork execution (for alternation)
 * - `CHOMSKY3_OP_MATCH` (0x22): Successful match
 * - `CHOMSKY3_OP_FAIL` (0x23): Force backtrack
 * 
 * **Anchors (0x30-0x3F)**
 * - `CHOMSKY3_OP_ANCHOR_START` (0x30): Match start of input ^
 * - `CHOMSKY3_OP_ANCHOR_END` (0x31): Match end of input $
 * - `CHOMSKY3_OP_ANCHOR_LINE_START` (0x32): Match line start (multiline)
 * - `CHOMSKY3_OP_ANCHOR_LINE_END` (0x33): Match line end (multiline)
 * - `CHOMSKY3_OP_ANCHOR_WORD_BOUNDARY` (0x34): Word boundary \b
 * - `CHOMSKY3_OP_ANCHOR_NON_WORD_BOUNDARY` (0x35): Non-boundary \B
 * 
 * **Capture Groups (0x40-0x4F)**
 * - `CHOMSKY3_OP_SAVE_START` (0x40): Save capture group start
 * - `CHOMSKY3_OP_SAVE_END` (0x41): Save capture group end
 * - `CHOMSKY3_OP_BACKREF` (0x42): Backreference \1, \2, etc.
 * 
 * **Lookahead/Lookbehind (0x50-0x5F)**
 * - `CHOMSKY3_OP_LOOK_AHEAD` (0x50): Positive lookahead (?=...)
 * - `CHOMSKY3_OP_LOOK_AHEAD_NEG` (0x51): Negative lookahead (?!...)
 * - `CHOMSKY3_OP_LOOK_BEHIND` (0x52): Positive lookbehind (?<=...)
 * - `CHOMSKY3_OP_LOOK_BEHIND_NEG` (0x53): Negative lookbehind (?<!...)
 * 
 * **Quantifiers (0x60-0x6F)**
 * - `CHOMSKY3_OP_REPEAT` (0x60): Greedy repeat {n,m}
 * - `CHOMSKY3_OP_REPEAT_LAZY` (0x61): Lazy repeat {n,m}?
 * - `CHOMSKY3_OP_REPEAT_NG` (0x62): Non-greedy repeat (possessive)
 * 
 * **Special (0xF0-0xFF)**
 * - `CHOMSKY3_OP_NOP` (0xF0): No operation
 * - `CHOMSKY3_OP_DEBUG` (0xFE): Debug breakpoint
 * - `CHOMSKY3_OP_CHECKPOINT` (0xFF): Execution checkpoint
 * 
 * @section vm_sec Virtual Machine
 * 
 * @subsection vm_execution Execution Model
 * 
 * The VM uses a **stack-based backtracking model**:
 * 
 * 1. **Program Counter (PC)**: Points to current instruction
 * 2. **Input Position**: Current position in input string
 * 3. **Backtrack Stack**: Stores alternative execution paths
 * 4. **Capture Slots**: Stores capture group boundaries
 * 
 * When a `SPLIT` instruction is encountered, the VM:
 * - Pushes the second branch to the backtrack stack
 * - Continues with the first branch
 * - On failure, pops the stack and tries the second branch
 * 
 * @subsection vm_config VM Configuration
 * 
 * @code{.c}
 * typedef struct {
 *     chomsky3_vm_mode_t mode;         // Execution mode
 *     chomsky3_vm_limits_t limits;     // Resource limits
 *     chomsky3_vm_callbacks_t callbacks; // Event callbacks
 *     void *user_data;                 // User context
 * } chomsky3_vm_config_t;
 * @endcode
 * 
 * @subsection vm_modes Execution Modes
 * 
 * - **NORMAL**: Standard execution, no instrumentation
 * - **TRACE**: Log each instruction executed
 * - **DEBUG**: Enable breakpoints and single-stepping
 * - **PROFILE**: Collect performance statistics
 * - **SAFE**: Extra validation and bounds checking
 * 
 * @subsection vm_limits Resource Limits
 * 
 * @code{.c}
 * typedef struct {
 *     uint64_t max_steps;          // Maximum instructions executed
 *     uint64_t max_backtracks;     // Maximum backtrack operations
 *     uint32_t max_stack_depth;    // Maximum backtrack stack depth
 *     uint64_t max_time_ms;        // Maximum execution time
 *     size_t   max_memory;         // Maximum memory allocation
 * } chomsky3_vm_limits_t;
 * @endcode
 * 
 * These limits prevent **ReDoS attacks** where malicious patterns cause
 * exponential backtracking.
 * 
 * @subsection vm_state VM State
 * 
 * @code{.c}
 * typedef struct {
 *     const char *input;           // Input string
 *     size_t input_length;         // Input length
 *     size_t position;             // Current position
 *     
 *     const char *match_start;     // Match start position
 *     const char *match_end;       // Match end position
 *     
 *     chomsky3_capture_t *captures; // Capture groups
 *     uint32_t capture_count;      // Number of captures
 *     
 *     uint64_t steps_executed;     // Instructions executed
 *     uint64_t backtracks;         // Backtrack operations
 * } chomsky3_vm_state_t;
 * @endcode
 * 
 * @section jit_sec JIT Compilation
 * 
 * @subsection jit_overview JIT Overview
 * 
 * The JIT compiler translates bytecode to native machine code using SLJIT:
 * 
 * 1. **Bytecode Analysis**: Identify hot paths and optimization opportunities
 * 2. **Code Generation**: Emit native instructions for each opcode
 * 3. **Register Allocation**: Map VM state to CPU registers
 * 4. **Optimization**: Inline common patterns, eliminate bounds checks
 * 5. **Code Emission**: Generate executable machine code
 * 
 * @subsection jit_benefits JIT Benefits
 * 
 * - **10-100x faster** than interpreter for complex patterns
 * - **Reduced memory traffic** by keeping state in registers
 * - **Inlined operations** eliminate function call overhead
 * - **Platform-specific optimizations** (SSE, AVX, NEON)
 * 
 * @subsection jit_api JIT API
 * 
 * @code{.c}
 * // Compile bytecode to native code
 * int chomsky3_jit_compile(const chomsky3_bytecode_t *bytecode,
 *                          chomsky3_jit_code_t **code,
 *                          const chomsky3_jit_config_t *config);
 * 
 * // Execute JIT-compiled code
 * int chomsky3_jit_execute(const chomsky3_jit_code_t *code,
 *                          chomsky3_vm_state_t *state,
 *                          const chomsky3_vm_config_t *config);
 * 
 * // Free JIT-compiled code
 * void chomsky3_jit_free(chomsky3_jit_code_t *code);
 * @endcode
 * 
 * @section api_sec API Reference
 * 
 * @subsection api_lifecycle Lifecycle Functions
 * 
 * @code{.c}
 * // Create VM instance
 * chomsky3_vm_t* chomsky3_vm_create(const chomsky3_vm_config_t *config);
 * 
 * // Destroy VM instance
 * void chomsky3_vm_destroy(chomsky3_vm_t *vm);
 * 
 * // Reset VM state
 * void chomsky3_vm_reset(chomsky3_vm_t *vm);
 * @endcode
 * 
 * @subsection api_execution Execution Functions
 * 
 * @code{.c}
 * // Execute bytecode
 * int chomsky3_vm_execute(const chomsky3_bytecode_t *bytecode,
 *                         chomsky3_vm_state_t *state,
 *                         const chomsky3_vm_config_t *config);
 * 
 * // Match pattern against input
 * int chomsky3_vm_match(const chomsky3_bytecode_t *bytecode,
 *                       const char *input,
 *                       size_t input_length,
 *                       chomsky3_vm_state_t *state,
 *                       const chomsky3_vm_config_t *config);
 * 
 * // Search for pattern in input
 * int chomsky3_vm_search(const chomsky3_bytecode_t *bytecode,
 *                        const char *input,
 *                        size_t input_length,
 *                        size_t start_pos,
 *                        chomsky3_vm_state_t *state,
 *                        const chomsky3_vm_config_t *config);
 * @endcode
 * 
 * @subsection api_bytecode Bytecode Functions
 * 
 * @code{.c}
 * // Create bytecode from instructions
 * chomsky3_bytecode_t* chomsky3_bytecode_create(
 *     const chomsky3_instruction_t *instructions,
 *     uint32_t instruction_count,
 *     const void *data,
 *     uint32_t data_size);
 * 
 * // Free bytecode
 * void chomsky3_bytecode_free(chomsky3_bytecode_t *bytecode);
 * 
 * // Validate bytecode
 * int chomsky3_bytecode_validate(const chomsky3_bytecode_t *bytecode);
 * 
 * // Serialize bytecode to buffer
 * int chomsky3_bytecode_serialize(const chomsky3_bytecode_t *bytecode,
 *                                 void *buffer,
 *                                 size_t buffer_size,
 *                                 size_t *bytes_written);
 * 
 * // Deserialize bytecode from buffer
 * int chomsky3_bytecode_deserialize(const void *buffer,
 *                                   size_t buffer_size,
 *                                   chomsky3_bytecode_t **bytecode);
 * @endcode
 * 
 * @section usage_sec Usage Examples
 * 
 * @subsection usage_basic Basic Matching
 * 
 * @code{.c}
 * #include <chomsky3/vm.h>
 * #include <chomsky3/bytecode.h>
 * 
 * // Compile pattern (simplified - actual compilation not shown)
 * chomsky3_bytecode_t *bytecode = compile_pattern("hello.*world");
 * 
 * // Configure VM
 * chomsky3_vm_config_t config = {
 *     .mode = CHOMSKY3_VM_MODE_NORMAL,
 *     .limits = {
 *         .max_steps = 1000000,
 *         .max_backtracks = 100000,
 *         .max_stack_depth = 1000
 *     }
 * };
 * 
 * // Execute match
 * chomsky3_vm_state_t state;
 * const char *input = "hello beautiful world";
 * int result = chomsky3_vm_match(bytecode, input, strlen(input), 
 *                                &state, &config);
 * 
 * if (result == CHOMSKY3_SUCCESS) {
 *     printf("Match: %.*s\n", 
 *            (int)(state.match_end - state.match_start),
 *            state.match_start);
 * }
 * 
 * chomsky3_bytecode_free(bytecode);
 * @endcode
 * 
 * @subsection usage_captures Capture Groups
 * 
 * @code{.c}
 * // Pattern: (\w+)@(\w+\.\w+)
 * chomsky3_bytecode_t *bytecode = compile_pattern("(\\w+)@(\\w+\\.\\w+)");
 * 
 * chomsky3_vm_state_t state;
 * const char *input = "user@example.com";
 * 
 * if (chomsky3_vm_match(bytecode, input, strlen(input), 
 *                       &state, &config) == CHOMSKY3_SUCCESS) {
 *     // Group 0: full match
 *     printf("Full: %.*s\n",
 *            (int)(state.match_end - state.match_start),
 *            state.match_start);
 *     
 *     // Group 1: username
 *     printf("User: %.*s\n",
 *            (int)(state.captures[0].end - state.captures[0].start),
 *            state.captures[0].start);
 *     
 *     // Group 2: domain
 *     printf("Domain: %.*s\n",
 *            (int)(state.captures[1].end - state.captures[1].start),
 *            state.captures[1].start);
 * }
 * @endcode
 * 
 * @subsection usage_jit JIT Compilation
 * 
 * @code{.c}
 * #include <chomsky3/jit.h>
 * 
 * // Compile bytecode to native code
 * chomsky3_jit_code_t *jit_code;
 * chomsky3_jit_config_t jit_config = {
 *     .optimization_level = 2,
 *     .enable_profiling = false
 * };
 * 
 * int result = chomsky3_jit_compile(bytecode, &jit_code, &jit_config);
 * if (result != CHOMSKY3_SUCCESS) {
 *     // Fall back to interpreter
 *     chomsky3_vm_execute(bytecode, &state, &config);
 * } else {
 *     // Execute JIT-compiled code (much faster)
 *     chomsky3_jit_execute(jit_code, &state, &config);
 *     chomsky3_jit_free(jit_code);
 * }
 * @endcode
 * 
 * @subsection usage_limits Resource Limits
 * 
 * @code{.c}
 * // Protect against ReDoS attacks
 * chomsky3_vm_config_t safe_config = {
 *     .mode = CHOMSKY3_VM_MODE_SAFE,
 *     .limits = {
 *         .max_steps = 10000,        // Limit instructions
 *         .max_backtracks = 1000,    // Limit backtracking
 *         .max_stack_depth = 100,    // Limit recursion
 *         .max_time_ms = 100,        // 100ms timeout
 *         .max_memory = 1024 * 1024  // 1MB memory limit
 *     }
 * };
 * 
 * int result = chomsky3_vm_match(bytecode, untrusted_input, 
 *                                input_len, &state, &safe_config);
 * 
 * if (result == CHOMSKY3_ERROR_RESOURCE_LIMIT) {
 *     printf("Pattern execution exceeded resource limits\n");
 * }
 * @endcode
 * 
 * @section error_sec Error Handling
 * 
 * @subsection error_codes Error Codes
 * 
 * - `CHOMSKY3_SUCCESS` (0): Operation successful
 * - `CHOMSKY3_ERROR_NO_MATCH` (-1): Pattern did not match
 * - `CHOMSKY3_ERROR_INVALID_ARGUMENT` (-2): Invalid function argument
 * - `CHOMSKY3_ERROR_INVALID_BYTECODE` (-3): Malformed bytecode
 * - `CHOMSKY3_ERROR_OUT_OF_MEMORY` (-4): Memory allocation failed
 * - `CHOMSKY3_ERROR_RESOURCE_LIMIT` (-5): Resource limit exceeded
 * - `CHOMSKY3_ERROR_INTERNAL` (-6): Internal error
 * - `CHOMSKY3_ERROR_NOT_IMPLEMENTED` (-7): Feature not implemented
 * 
 * @section perf_sec Performance Considerations
 * 
 * @subsection perf_interpreter Interpreter Performance
 * 
 * - **Typical speed**: 10-50 MB/s for simple patterns
 * - **Backtracking overhead**: Exponential worst case
 * - **Memory usage**: O(pattern_size + input_size)
 * 
 * @subsection perf_jit JIT Performance
 * 
 * - **Typical speed**: 100-500 MB/s for simple patterns
 * - **Compilation overhead**: 1-10ms per pattern
 * - **Best for**: Patterns executed many times
 * 
 * @subsection perf_optimization Optimization Tips
 * 
 * 1. **Use anchors**: `^` and `$` reduce search space
 * 2. **Avoid nested quantifiers**: `(a*)*` causes exponential backtracking
 * 3. **Use atomic groups**: `(?>...)` prevents backtracking
 * 4. **Enable JIT**: For hot patterns executed repeatedly
 * 5. **Set resource limits**: Protect against malicious patterns
 * 
 * @section thread_sec Thread Safety
 * 
 * - **Bytecode objects**: Immutable, safe to share across threads
 * - **VM instances**: Not thread-safe, one per thread
 * - **JIT code**: Immutable, safe to share across threads
 * - **VM state**: Thread-local, never shared
 * 
 * @section compat_sec Compatibility
 * 
 * @subsection compat_regex Regex Syntax Support
 * 
 * Chomsky3 supports a subset of PCRE/Perl regex syntax:
 * 
 * - ✅ Character classes: `[abc]`, `[^abc]`, `[a-z]`
 * - ✅ Quantifiers: `*`, `+`, `?`, `{n}`, `{n,}`, `{n,m}`
 * - ✅ Anchors: `^`, `$`, `\b`, `\B`
 * - ✅ Alternation: `a|b`
 * - ✅ Capture groups: `(...)`, `\1`, `\2`
 * - ✅ Non-capturing groups: `(?:...)`
 * - ✅ Lookahead: `(?=...)`, `(?!...)`
 * - ✅ Lookbehind: `(?<=...)`, `(?<!...)`
 * - ✅ Escape sequences: `\d`, `\w`, `\s`, `\n`, `\t`
 * - ❌ Backreferences in lookahead (not supported)
 * - ❌ Recursive patterns (not supported)
 * - ❌ Unicode properties (planned)
 * 
 * @subsection compat_platforms Platform Support
 * 
 * - **Linux**: x86-64, ARM64, ARM32
 * - **Windows**: x86-64, ARM64
 * - **macOS**: x86-64, ARM64 (Apple Silicon)
 * - **BSD**: x86-64, ARM64
 * 
 * @section build_sec Building
 * 
 * @subsection build_cmake CMake Build
 * 
 * @code{.sh}
 * mkdir build && cd build
 * cmake .. -DCMAKE_BUILD_TYPE=Release
 * make -j$(nproc)
 * make install
 * @endcode
 * 
 * @subsection build_options Build Options
 * 
 * - `CHOMSKY3_ENABLE_JIT`: Enable JIT compilation (default: ON)
 * - `CHOMSKY3_ENABLE_PROFILING`: Enable profiling support (default: OFF)
 * - `CHOMSKY3_ENABLE_TESTS`: Build test suite (default: ON)
 * - `CHOMSKY3_ENABLE_BENCHMARKS`: Build benchmarks (default: OFF)
 * 
 * @section license_sec License
 * 
 * Chomsky3 is released under the MIT License.
 * 
 * @section credits_sec Credits
 * 
 * - **SLJIT**: Portable JIT compiler by Zoltan Herczeg
 * - **DParser**: Parser generator by John Plevyak
 * 
 * @author Chomsky3 Development Team
 * @version 1.0.0
 * @date 2026
 */
