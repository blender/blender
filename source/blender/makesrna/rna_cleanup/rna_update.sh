cd ../../../../
./blender.bin --background --python ./release/scripts/modules/rna_info.py 2> source/blender/makesrna/rna_cleanup/out.txt
cd ./source/blender/makesrna/rna_cleanup/
./rna_cleaner.py out.txt
./rna_cleaner.py rna_properties.txt
./rna_cleaner_merge.py out_work.py rna_properties_work.py
./rna_cleaner.py out_work_merged.py
./rna_cleaner.py out_work_lost.py
mv out_work_merged_work.txt rna_properties_new.txt
mv out_work_lost_work.txt rna_properties_lost.txt
echo "Updated: rna_properties_new.txt rna_properties_lost.txt"