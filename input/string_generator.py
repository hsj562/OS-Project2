import random

random_dict = "1234567890qwertyuiopasdfghjklzxcvbnm"

byte_num = int(input('how many byte:'))
to_write = ""
for i in range(byte_num):
	r = random.randint(0, len(random_dict)-1)
	to_write += random_dict[r]
with open('our_input.txt', 'w') as f:
	f.write(to_write)
