#!/usr/bin/env python3.2

import sys

'''
Example usage:
 python3 rna_cleaner_merge.py out_work.py rna_booleans_work.py
'''
def main():
	
	def work_line_id(line):
		return line[2].split("|")[-1], line[3] # class/from
	
	
	if not (sys.argv[-1].endswith(".py") and sys.argv[-2].endswith(".py")):
		print("Only accepts 2 py files as arguments.")
	
	sys.path.insert(0, ".")

	mod_from = __import__(sys.argv[-1][:-3])
	mod_to = __import__(sys.argv[-2][:-3])
	
	mod_to_dict = dict([(work_line_id(line), line) for line in mod_to.rna_api])
	mod_from_dict = dict([(work_line_id(line), line) for line in mod_from.rna_api])
	
	rna_api_new = []
	
	for key, val_orig in mod_to_dict.items():
		try:
			val_new = mod_from_dict.pop(key)
		except:
			# print("not found", key)
			val_new = val_orig
			
		# always take the class from the base
		val = list(val_orig)
		val[0] = val_new[0] # comment
		val[4] = val_new[4] # -> to
		val = tuple(val)
		rna_api_new.append(val)
	
	def write_work_file(file_path, rna_api):
		rna_api = list(rna_api)
		rna_api.sort(key=work_line_id)
		file_out = open(file_path, "w")
		file_out.write("rna_api = [\n")
		for line in rna_api:
			file_out.write("    %s,\n" % (repr(line)))
		file_out.write("]\n")
		file_out.close()

	file_path = sys.argv[-2][:-3] + "_merged.py"
	write_work_file(file_path, rna_api_new)
	
	if mod_from_dict:
		file_path = sys.argv[-2][:-3] + "_lost.py"
		write_work_file(file_path, list(mod_from_dict.values()))
		print("Warning '%s' contains lost %d items from module %s.py" % (file_path, len(mod_from_dict), mod_from.__name__))

if __name__ == "__main__":
	main()
