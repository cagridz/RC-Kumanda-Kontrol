FLYSKY FS-İ6X 2A uzaktan kumanda ile Arduino Mega kontrollü 2WD Robot kontrolü için yazılmıştır. 
Daha sonra eklenecek olan Lidar, Lora, Ultrasonik sensör, GPS modülü de koda eklenmiştir. Aşağıda FS-İA6B alıcısının bağlantı şeması yer almaktadır. 

Devre 3S 18650 Lityum batarya ile beslenmektedir. Pil kontrolü için BMS kullanılmıştır. Bataryaya 2 adet DC-DC voltaj düşürücü bağlıdır. Biri doğrudan motoru beslemektedir. Motor için 6V 250 Rpm tip kullanılmış olup, motor sürücü için L298N çift motor sürücü kullanılmıştır ve ayrı beslenmektedir. Motor gerilimine bağlı olarak besleme gerilimi 6V ile sınırlandırılmıştır. Diğer DC-DC voltaj düşürücü ise Arduino Mega ve Alıcıyı beslemektedir. 

![Adsız](https://github.com/user-attachments/assets/c04d3a5e-754d-4b1a-904a-37d8105bb675)
![Screenshot_2](https://github.com/user-attachments/assets/11c4ba71-b663-402f-8b4d-0787ecf590da)

Robotun önüne ve arkasına yerleştirilen HC - SR04 sensörü, hareket edilen taraftaki engeli algılayıp durması gerektiği için buna göre ayrı ayrı değerlendirilmiştir. Kumanda edildiği yönde engel algıladığı zaman durmaktadır. 


![WhatsApp Image 2025-03-24 at 00 35 06](https://github.com/user-attachments/assets/83aece89-1041-456e-9448-e43fec12633e)


Mesafe sensörleri 20 cm de durmaktadır. 
