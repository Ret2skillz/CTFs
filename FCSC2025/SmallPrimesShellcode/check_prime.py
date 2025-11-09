import math
from keystone import *

def isPrime(n):
    """
    Replicates the C function logic exactly.
    Checks if a number is prime.
    """
    if n <= 1:
        return False
    if n == 2 or n == 3:
        return True
    if n % 2 == 0 or n % 3 == 0:
        return False
    
    i = 5
    while i * i <= n:
        if n % i == 0 or n % (i + 2) == 0:
            return False
        i += 6
    
    return True

def assemble_instruction(instruction):
    """
    Assemble an ARM64 instruction and return its opcode.
    Returns None if assembly fails.
    """
    try:
        ks = Ks(KS_ARCH_ARM64, KS_MODE_LITTLE_ENDIAN)
        encoding, count = ks.asm(instruction)
        if encoding and len(encoding) == 4:
            # Convert bytes to 32-bit integer (little endian)
            opcode = int.from_bytes(bytes(encoding), byteorder='little')
            return opcode
        return None
    except Exception as e:
        return None

def check_arm64_instructions():
    """
    Check various ARM64 instructions to see which ones have prime opcodes.
    """
    print("Checking ARM64 Instructions for Prime Opcodes")
    print("=" * 80)
    print()
    
    # Common ARM64 instructions to test
    instructions = [
        # Data movement
        "svc #1",
        "svc #2",
        "svc #3",
        "svc #4",
        "svc #5",
        "svc #6",
        "svc #7",
        "svc #8",
        "svc #9",
        "svc #10",
        "svc #11",
        "svc #12",
        "svc #13",
        "svc #14",
        "svc #15",
        "svc #16",
        "svc #17",
        "svc #18",
        "svc #19",
        "svc #20",
        "ldp x1, x8",
        "mov sp, x0",
        "nop",
        "0x000000006e69622f",
        "0x68732f2f00000005"
    ]
    
    # Expand with more register combinations
    expanded_instructions = instructions.copy()
    
    # Add more register variations
    for i in range(32):
        expanded_instructions.append(f"mov x{i}, x{i}")
        expanded_instructions.append(f"mov x0, x{i}")
        expanded_instructions.append(f"add x0, x0, x{i}")
        expanded_instructions.append(f"ldr x{i}, [sp]")
        expanded_instructions.append(f"str x{i}, [sp]")
    
    # Add immediate variations
    for imm in range(1000):
        expanded_instructions.append(f"add x0, x0, #{imm}")
        expanded_instructions.append(f"sub x0, x0, #{imm}")
        expanded_instructions.append(f"add x1, x1, #{imm}")
        expanded_instructions.append(f"add x1, x1, x{imm}")
        expanded_instructions.append(f"add x3, x3, #{imm}")
        expanded_instructions.append(f"add x15, x15, #{imm}")
        expanded_instructions.append(f"add x3, x3, x{imm}")
        expanded_instructions.append(f"sub x1, x1, #{imm}")
        expanded_instructions.append(f"sub x3, x3, #{imm}")
        expanded_instructions.append(f"sub x15, x15, #{imm}")
        expanded_instructions.append(f"str x1, [sp, #{imm}]")
        expanded_instructions.append(f"str x0, [sp, #{imm}]")
        expanded_instructions.append(f"ldp x0, x8, [sp, #{imm}]")
        expanded_instructions.append(f"ldp x1, x8, [sp, #{imm}]")
        expanded_instructions.append(f"ldr x1, [sp, #{imm}]")
        expanded_instructions.append(f"ldr x3, [sp, #{imm}]")
        expanded_instructions.append(f"ldr x15, [sp, #{imm}]")
        expanded_instructions.append(f"ldr x18, [sp, #{imm}]")
        expanded_instructions.append(f"ldr x21, [sp, #{imm}]")
        expanded_instructions.append(f"str x1, [sp, #{imm}]")
        expanded_instructions.append(f"movz x0, #{imm}")
        expanded_instructions.append(f"movk x0, #{imm}")
        
    
    prime_instructions = []
    non_prime_instructions = []
    failed_instructions = []
    
    print("Assembling and checking instructions...")
    print()
    
    for instr in expanded_instructions:
        opcode = assemble_instruction(instr)
        if opcode is None:
            failed_instructions.append(instr)
            continue
        
        is_prime = isPrime(opcode)
        if is_prime:
            prime_instructions.append((instr, opcode))
        else:
            non_prime_instructions.append((instr, opcode))
    
    # Display results
    print(f"Total instructions tested: {len(expanded_instructions)}")
    print(f"Successfully assembled: {len(prime_instructions) + len(non_prime_instructions)}")
    print(f"Failed to assemble: {len(failed_instructions)}")
    print(f"Prime opcodes found: {len(prime_instructions)}")
    print(f"Non-prime opcodes: {len(non_prime_instructions)}")
    print()
    
    if prime_instructions:
        print("=" * 80)
        print("INSTRUCTIONS WITH PRIME OPCODES:")
        print("=" * 80)
        for instr, opcode in sorted(prime_instructions, key=lambda x: x[1]):
            print(f"  0x{opcode:08x} ({opcode:>10d})  {instr}")
        print()
    else:
        print("No instructions with prime opcodes found!")
        print()
    
    # Show some non-prime examples
    print("=" * 80)
    print("SAMPLE NON-PRIME OPCODES (first 20):")
    print("=" * 80)
    for instr, opcode in non_prime_instructions[:20]:
        print(f"  0x{opcode:08x} ({opcode:>10d})  {instr}")
    print()
    
    return prime_instructions, non_prime_instructions

def check_custom_instruction(instruction):
    """
    Check if a specific instruction has a prime opcode.
    """
    print(f"\nChecking: {instruction}")
    opcode = assemble_instruction(instruction)
    if opcode is None:
        print(f"  ERROR: Failed to assemble instruction")
        return None
    
    is_prime = isPrime(opcode)
    print(f"  Opcode: 0x{opcode:08x} ({opcode})")
    print(f"  Result: {'✓ PRIME' if is_prime else '✗ NOT PRIME'}")
    return is_prime

# Main execution
if __name__ == "__main__":
    try:
        print("\n")
        print("=" * 80)
        print("ARM64 INSTRUCTION PRIME OPCODE CHECKER")
        print("=" * 80)
        print("\nThis tool checks which ARM64 instructions have prime opcodes.")
        print("It uses the Keystone assembler engine.\n")
        
        prime_instrs, non_prime_instrs = check_arm64_instructions()
        
        print("=" * 80)
        print("ANALYSIS COMPLETE")
        print("=" * 80)
        print(f"\nPrime instruction percentage: {len(prime_instrs)/(len(prime_instrs)+len(non_prime_instrs))*100:.2f}%")
        
        # Show statistics about prime distribution
        if prime_instrs:
            opcodes = [op for _, op in prime_instrs]
            print(f"Smallest prime opcode: 0x{min(opcodes):08x} ({min(opcodes)})")
            print(f"Largest prime opcode: 0x{max(opcodes):08x} ({max(opcodes)})")
        
        print("\n" + "=" * 80)
        print("You can test custom instructions with:")
        print("  check_custom_instruction('mov x5, x7')")
        print("=" * 80 + "\n")
        
    except ImportError:
        print("\nERROR: Keystone engine not found!")
        print("Install with: pip install keystone-engine")
        print("\nAlternatively, you can manually provide instruction encodings.")
