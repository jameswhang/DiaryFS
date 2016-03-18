sudo umount /home/james/test/dir1
sudo umount /home/james/test
sudo rmmod diaryfs
sudo insmod diaryfs.ko
sudo mount -t ext4 /dev/sda3 /home/james/test
sudo mount -t diaryfs /home/james/test/dir1 /home/james/test/dir2
sudo echo hello > /home/james/test/dir2/test.txt
