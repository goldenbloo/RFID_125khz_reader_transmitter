def int_to_nibbles(n):
    # Convert the integer to binary string
    binary_str = bin(n)[2:]
    
    # Pad the binary string with zeros to make its length a multiple of 4
    # binary_str = binary_str.zfill(4 * ((len(binary_str) + 3) // 4))
    binary_str = binary_str.zfill(40)
    
    # Split the binary string into nibbles and convert them to integers
    nibbles = [int(binary_str[i:i+4], 2) for i in range(0, len(binary_str), 4)]
    
    return nibbles

def calculate_parity(nibble):
    # Convert the nibble to binary string
    binary_str = bin(nibble)[2:].zfill(4)
    
    # Count the number of 1-bits
    count = binary_str.count('1')
    
    # Check if the count is even or odd
    if count % 2 == 0:
        return 0
    else:
        return 1

f = open("manchester_rfid.txt", 'w')
f.write("HHHHHHHHH")
tag = 0xabba123456
#-------xxxxxxxxxx
nibbles =  int_to_nibbles(tag)
nibbles_with_parity = list(map(lambda n: (n<<1) | calculate_parity(n),nibbles))
print([bin(n)[2:].zfill(5) for n in nibbles])
print([bin(n)[2:].zfill(5) for n in nibbles_with_parity])
column_parity = 0
for n in nibbles:
    column_parity ^= n
column_parity <<= 1
print(bin(column_parity)[2:].zfill(5))
# f.write("1111111110000001100000000000000011001010101010010111010011001000".replace('1','H').replace('0','L'))
f.write(''.join([bin(n)[2:].zfill(5).replace('1','H').replace('0','L') for n in nibbles_with_parity]))
f.write(''.join(bin(column_parity)[2:].zfill(5).replace('1','H').replace('0','L')))

f.close()