# slidingwindow
Tugas Besar 1 IF3130 Jaringan Komputer

Untuk menjalankan program recvfile dan sendfile, lakukan:
1. Buka terminal dan masukkan command make untuk compile.
2. Eksekusi ./recvfile dengan masukan parameter <filename> <windowsize> <buffersize> <port>
  Misalnya, ./recvfile output/test.txt 30 256 8080 berarti menerima file dan menulisnya ke output/test.txt dengan receiving window size = 30, buffer size = 256, diterima di port 8080.
3. Eksekusi ./sendfile dengan masukan parameter <filename> <windowsize> <buffersize> <destination_ip> <destination_port>
  Misalnya, ./sendfile input/test.txt 25 256 127.0.0.1 8080 berarti mengirim file input/test.txt dengan window size = 25, buffer size = 256, ke alamat dengan IP address 127.0.0.1 ke port 8080.

Cara kerja sliding window pada sender:
1. Maintain nilai dari LFS (Last Frame Sent) dan LAR (Last Acknowledge Receive), sehingga nilai LFS-LAR <= windowsize
2. Selama (LAR < banyak paket) :
	1) Pertama, Pengecekan paket yang Timeout. Jika ada yang Timeout, lalu update timeoutnya
	2) Kedua, Pengecekan window, jika window<windowsize, perbesar window dengan mengirimpaket-paket selanjutnya lalu tambahkan timeout
	3) Ketiga, Pengecekan apakah terdapat ACK yang telah dikirim, jika ada maka upyda nilai LAR. Jika diterima NAK, maka update timeout seq num dengan 0

Cara kerja sliding window pada receiver:
Selama belum diterimanya packet dengan ukuran 0,
1. Maintain nilai dari LFR (Last FrameReceive) dan LAF (Last Acceptable Frame)
2. Pertama, Pembacaan socket jika ada packet, cek seqnum dalam range window dan belum dilakukan penyimpanan akan dilakukan penyimpananx.
3. Maka kirim ack jika diterima ketika frame yang diterima sempurna atau kirim nak jika file yang diterima tidak sesuai (corrupt)
3. Kedua, penyesuaian LFR dari masukan yang telah diterima 

Pembagian tugas
1. Tony (13516010) : Sliding Window Receiver maupun di Sender
2. Nella Zabrina Pratama (13516025) : Receiver and Sliding Window di Receiver
3. Harry Setiawan Hamjaya (13516079) : Sender and Sliding Window di Sender