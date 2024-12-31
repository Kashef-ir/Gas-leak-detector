// شامل کردن کتابخانه Watchdog Timer (WDT) برای بازیابی سیستم در صورتی که برنامه متوقف شود
#include <avr/wdt.h>

// تعریف پایه حسگر به عنوان A0 برای خواندن داده‌های حسگر گاز (ورودی آنالوگ)
#define sensor_pin A0

// شامل کردن کتابخانه EEPROM برای خواندن و نوشتن در حافظه غیر فرار آردوینو (EEPROM)
#include <EEPROM.h>


void setup() {
  int i = 0; // مقداردهی اولیه متغیر شمارنده

  // شروع ارتباط سریال با نرخ بود 115200
  Serial.begin(115200);

  // فراخوانی تابعی برای بررسی و برقراری اتصال به شبکه GSM
  check_connect();

  // تنظیم ماژول GSM برای کار در حالت پیامک متنی
  Serial.print("AT+CMGF=1\r\n");
  delay(1000); // انتظار به مدت ۱ ثانیه برای اعمال دستور

  // حذف تمام پیامک‌های ذخیره شده در ماژول GSM
  Serial.print("AT+CMGD=1,4\r\n");
  delay(1000);

  // تنظیم پارامترهای پیامک (مانند حالت PDU و دوره اعتبار)
  Serial.print("AT+CSMP=17,167,0,0\r\n");
  delay(1000);

  // فعال کردن نمایش شماره تماس گیرنده
  Serial.print("AT+CLIP=1\r\n");
  delay(1000);

  // پاک کردن داده‌های باقی‌مانده در بافر سریال
  Serial.readString();

  // منتظر ماندن تا زمانی که حداقل یک شماره تلفن در EEPROM ذخیره شود
  // شماره‌ها در آدرس‌های 0 و 20 در EEPROM ذخیره می‌شوند
  while (char(EEPROM.read(0)) == NULL && char(EEPROM.read(20)) == NULL) {
    check_sms(); // به صورت مداوم بررسی پیامک‌ها برای دریافت تنظیمات
  }

  // ارسال پیامک برای اطلاع‌رسانی به کاربر که سیستم آماده دریافت تنظیمات است
  send_sms("READY TO RECEIVE SETTING!");

  // ورود به یک حلقه برای بررسی تنظیمات اضافی
  while (true) {
    delay(1000); // انتظار به مدت ۱ ثانیه
    check_sms(); // به صورت مداوم بررسی پیامک‌ها

    i++; // افزایش شمارنده

    // خروج از حلقه اگر شماره‌ای در EEPROM ذخیره شده باشد
    // یا ۱۲۰ ثانیه (۲ دقیقه) گذشته باشد
    if ((i >= 120) && (char(EEPROM.read(0)) != NULL || char(EEPROM.read(20)) != NULL))
      break;
  }

  // اطلاع‌رسانی به کاربر که سیستم تشخیص نشتی گاز شروع به کار کرده است
  send_sms("GAS LEAK DETECTOR BOOTED!");

  // انتظار به مدت ۵ دقیقه قبل از شروع پایش
  // این کار اطمینان می‌دهد که سیستم پایدار شده و کاربر زمان کافی برای تنظیمات اولیه دارد
  for (i = 0; i < 300; i++)
    delay(1000); // انتظار به مدت ۱ ثانیه (۳۰۰ ثانیه = ۵ دقیقه)

  // چاپ پیام به مانیتور سریال برای اعلام شروع پایش
  Serial.println("MONITORING");

  // خالی کردن بافر سریال برای اطمینان از حذف داده‌های باقی‌مانده
  Serial.flush();
}
void loop() {
  // خواندن مقدار آنالوگ از حسگر گاز
  // مقایسه با مقدار آستانه ۳۵۰
  if (analogRead(sensor_pin) > 350) {
    // اگر غلظت گاز از آستانه عبور کند،
    // به شماره کاربر(ها) ذخیره شده در EEPROM زنگ زده شود
    call_user();

    // بررسی تماس‌های ورودی در یک بازه زمانی مشخص
    // این کار به کاربر اجازه تعامل با سیستم را می‌دهد
    check_incoming_call();
  }
}
void check_connect() {
  String status_ = ""; // رشته‌ای برای ذخیره وضعیت پاسخ شبکه

  // اطلاع‌رسانی به کاربر که سیستم در حال انتظار برای اتصال به شبکه GSM است
  Serial.println("WAITING TO CONNECT TO NETWORK");

  // پاک کردن داده‌های باقی‌مانده در بافر سریال برای اطمینان از ارتباط تمیز
  Serial.flush();

  // بررسی پیوسته برای اتصال به شبکه
  while (true) {
    // اگر داده‌ای در بافر سریال وجود دارد، آن را بخوانید و نادیده بگیرید
    if (Serial.available() > 0)
      Serial.readString();

    // ارسال دستور AT برای بررسی وضعیت ثبت‌نام شبکه
    Serial.print("AT+CCALR?\r\n");

    // انتظار برای پاسخ ماژول GSM
    delay(1000);

    // خواندن پاسخ از ماژول GSM
    status_ = Serial.readString();

    // بررسی اگر پاسخ نشان‌دهنده ثبت‌نام موفق در شبکه باشد
    // "+CCALR: 1" به معنای ثبت‌نام موفق در شبکه است
    if (status_.indexOf("+CCALR: 1") >= 0)
      break; // خروج از حلقه در صورت اتصال
  }

  // چاپ پیام نشان‌دهنده اتصال موفق به شبکه
  Serial.println("CONNECTED TO NETWORK");

  // دوباره پاک کردن بافر سریال برای اطمینان از حذف داده‌های باقی‌مانده
  Serial.flush();
}
void check_sms() {
  String get_sms = ""; // متغیری برای ذخیره محتوای پیامک دریافتی

  // بررسی اگر داده‌ای در بافر سریال موجود باشد
  if (Serial.available() > 0)
    get_sms = Serial.readString(); // خواندن داده‌های ورودی سریال به داخل رشته

  // بررسی اگر داده دریافتی شامل اعلان پیامک باشد (+CMTI)
  if (get_sms.indexOf("+CMTI") >= 0) {
    // پاک کردن داده‌های اضافی در بافر سریال
    if (Serial.available() > 0)
      Serial.readString();

    // ارسال دستور AT برای خواندن اولین پیامک موجود در صندوق ورودی
    Serial.print("AT+CMGR=1\r\n");

    // انتظار تا زمانی که پاسخی از ماژول GSM دریافت شود
    while (Serial.available() == 0) {}

    // خواندن محتوای پیامک از بافر سریال
    get_sms = Serial.readString();

    // استخراج محتوای اصلی پیامک با استفاده از جداکننده‌های '!' و '#'
    get_sms = get_sms.substring(get_sms.indexOf("!") + 1, get_sms.indexOf("#"));

    // ارسال دستور AT برای حذف تمام پیامک‌های موجود در صندوق ورودی جهت آزادسازی حافظه
    Serial.print("AT+CMGD=1,4\r\n");
    delay(1000);

    // بررسی اگر محتوای پیامک یک دستور حذف باشد ("D1" یا "D2")
    if (get_sms == "D1" || get_sms == "D2")
      delete_number(get_sms); // حذف شماره متناظر از EEPROM
    else if (get_sms != "")
      save_number(get_sms); // ذخیره شماره جدید در EEPROM اگر محتوا معتبر باشد
  }
}
void check_connect() {
  String status_ = ""; // رشته‌ای برای ذخیره وضعیت پاسخ شبکه

  // اطلاع‌رسانی به کاربر که سیستم در حال انتظار برای اتصال به شبکه GSM است
  Serial.println("WAITING TO CONNECT TO NETWORK");

  // پاک کردن داده‌های باقی‌مانده در بافر سریال برای اطمینان از ارتباط تمیز
  Serial.flush();

  // بررسی پیوسته برای اتصال به شبکه
  while (true) {
    // اگر داده‌ای در بافر سریال وجود دارد، آن را بخوانید و نادیده بگیرید
    if (Serial.available() > 0)
      Serial.readString();

    // ارسال دستور AT برای بررسی وضعیت ثبت‌نام شبکه
    Serial.print("AT+CCALR?\r\n");

    // انتظار برای پاسخ ماژول GSM
    delay(1000);

    // خواندن پاسخ از ماژول GSM
    status_ = Serial.readString();

    // بررسی اگر پاسخ نشان‌دهنده ثبت‌نام موفق در شبکه باشد
    // "+CCALR: 1" به معنای ثبت‌نام موفق در شبکه است
    if (status_.indexOf("+CCALR: 1") >= 0)
      break; // خروج از حلقه در صورت اتصال
  }

  // چاپ پیام نشان‌دهنده اتصال موفق به شبکه
  Serial.println("CONNECTED TO NETWORK");

  // دوباره پاک کردن بافر سریال برای اطمینان از حذف داده‌های باقی‌مانده
  Serial.flush();
}
void check_sms() {
  String get_sms = ""; // متغیری برای ذخیره محتوای پیامک دریافتی

  // بررسی اگر داده‌ای در بافر سریال موجود باشد
  if (Serial.available() > 0)
    get_sms = Serial.readString(); // خواندن داده‌های ورودی سریال به داخل رشته

  // بررسی اگر داده دریافتی شامل اعلان پیامک باشد (+CMTI)
  if (get_sms.indexOf("+CMTI") >= 0) {
    // پاک کردن داده‌های اضافی در بافر سریال
    if (Serial.available() > 0)
      Serial.readString();

    // ارسال دستور AT برای خواندن اولین پیامک موجود در صندوق ورودی
    Serial.print("AT+CMGR=1\r\n");

    // انتظار تا زمانی که پاسخی از ماژول GSM دریافت شود
    while (Serial.available() == 0) {}

    // خواندن محتوای پیامک از بافر سریال
    get_sms = Serial.readString();

    // استخراج محتوای اصلی پیامک با استفاده از جداکننده‌های '!' و '#'
    get_sms = get_sms.substring(get_sms.indexOf("!") + 1, get_sms.indexOf("#"));

    // ارسال دستور AT برای حذف تمام پیامک‌های موجود در صندوق ورودی جهت آزادسازی حافظه
    Serial.print("AT+CMGD=1,4\r\n");
    delay(1000);

    // بررسی اگر محتوای پیامک یک دستور حذف باشد ("D1" یا "D2")
    if (get_sms == "D1" || get_sms == "D2")
      delete_number(get_sms); // حذف شماره متناظر از EEPROM
    else if (get_sms != "")
      save_number(get_sms); // ذخیره شماره جدید در EEPROM اگر محتوا معتبر باشد
  }
}
void save_number(String number) {
  int i = 0; // متغیری برای آدرس EEPROM
  int j = 0; // متغیری برای اندیس آرایه کاراکترها
  char buff[20]; // بافری برای نگهداری شماره تلفن به صورت آرایه کاراکتر

  // تبدیل شماره تلفن از نوع رشته به آرایه کاراکتر
  number.toCharArray(buff, 20);

  // بررسی اگر اسلات اول EEPROM خالی باشد
  if (char(EEPROM.read(0)) == NULL) {
    // ذخیره طول شماره در آدرس اول (0)
    EEPROM.write(0, number.length());

    // نوشتن هر کاراکتر شماره در آدرس‌های بعدی EEPROM
    for (i = 0; i < number.length(); i++) {
      EEPROM.write(i + 1, char(buff[i])); // ذخیره شماره تلفن از آدرس 1
    }
  } else {
    // اگر اسلات اول پر باشد، ذخیره شماره در اسلات دوم
    // ذخیره طول شماره در آدرس 20
    EEPROM.write(20, number.length());

    // نوشتن هر کاراکتر شماره در آدرس‌های بعدی از 21
    for (i = 20; i < number.length() + 20; i++) {
      EEPROM.write(i + 1, char(buff[j])); // ذخیره شماره تلفن
      j++; // افزایش اندیس آرایه کاراکتر
    }
  }
}

void send_sms(String text) {
  String number = ""; // متغیری برای ذخیره شماره مقصد پیامک
  String cmgs = "AT+CMGS=\""; // دستور برای ارسال پیامک، شروع با فرمت پایه

  // بررسی اگر شماره‌ای در اسلات اول EEPROM ذخیره شده باشد
  if (char(EEPROM.read(0)) != NULL) {
    number = read_number(0); // خواندن شماره از اسلات اول (شروع از آدرس 0)
    cmgs = cmgs + number + "\"\r\n"; // تکمیل دستور ارسال پیامک با شماره
    Serial.print(cmgs); // ارسال دستور به ماژول GSM
    delay(500); // انتظار برای پردازش دستور
    Serial.println(text); // ارسال محتوای پیامک
    delay(500); // زمان برای انتقال محتوای پیامک
    Serial.write(0x1a); // ارسال کاراکتر CTRL+Z (0x1A) برای پایان پیامک
    delay(15000); // انتظار برای ارسال موفق پیامک
  }
  // اگر اسلات اول خالی بود، بررسی اسلات دوم
  else if (char(EEPROM.read(20)) != NULL) {
    cmgs = "AT+CMGS=\""; // تنظیم مجدد دستور پایه ارسال پیامک
    number = read_number(20); // خواندن شماره از اسلات دوم (شروع از آدرس 20)
    cmgs = cmgs + number + "\"\r\n"; // تکمیل دستور ارسال پیامک با شماره
    Serial.print(cmgs); // ارسال دستور به ماژول GSM
    delay(500); // انتظار برای پردازش دستور
    Serial.println(text); // ارسال محتوای پیامک
    Serial.write(0x1a); // ارسال کاراکتر CTRL+Z (0x1A) برای پایان پیامک
  }
}

String read_number(int addr) {
  String number_to_sms = ""; // رشته‌ای برای ذخیره شماره خوانده شده از EEPROM
  int len = int(EEPROM.read(addr)); // خواندن طول شماره ذخیره شده در آدرس مورد نظر

  // حلقه برای بازیابی کاراکترهای شماره از EEPROM
  for (int i = addr + 1; i <= len + addr; i++) {
    number_to_sms.concat(char(EEPROM.read(i))); // خواندن هر کاراکتر و افزودن به رشته
  }

  return number_to_sms; // بازگشت شماره بازسازی‌شده به صورت رشته
}

void delete_number(String del) {
  int len = 0; // متغیری برای ذخیره طول شماره حذف‌شده

  // بررسی اگر کاربر بخواهد شماره ذخیره شده اول ("D1") را حذف کند
  if (del == "D1") {
    len = EEPROM.read(0); // خواندن طول شماره ذخیره شده در آدرس 0
    for (int i = 0; i <= len; i++) {
      EEPROM.write(i, NULL); // بازنویسی شماره و طول آن با NULL
    }
  }
  // بررسی اگر کاربر بخواهد شماره ذخیره شده دوم ("D2") را حذف کند
  else if (del == "D2") {
    len = EEPROM.read(20); // خواندن طول شماره ذخیره شده در آدرس 20
    for (int i = 20; i <= len + 20; i++) {
      EEPROM.write(i, NULL); // بازنویسی شماره و طول آن با NULL
    }
  }
}

void call_user() {
  String number = "ATD"; // دستور پایه AT برای برقراری تماس

  // بررسی اگر شماره‌ای در اسلات اول EEPROM ذخیره شده باشد
  if (EEPROM.read(0) != NULL) {
    number = number + read_number(0) + ";\r\n"; // افزودن شماره ذخیره شده به دستور
    Serial.print(number); // ارسال دستور AT به ماژول GSM برای برقراری تماس
    delay(20000); // انتظار 20 ثانیه برای زنگ خوردن تماس
  }

  // بررسی اگر شماره‌ای در اسلات دوم EEPROM ذخیره شده باشد
  if (EEPROM.read(20) != NULL) {
    number = "ATD" + read_number(20) + ";\r\n"; // افزودن شماره ذخیره شده دوم به دستور
    Serial.print(number); // ارسال دستور AT به ماژول GSM برای برقراری تماس
    delay(20000); // انتظار 20 ثانیه برای زنگ خوردن تماس
  }
}

void check_incoming_call() {
  String incoming_number = ""; // متغیری برای ذخیره داده‌های ورودی از ماژول GSM
  unsigned long tm = millis(); // ثبت زمان فعلی برای مدیریت مدت زمان انتظار

  // حلقه برای حداکثر مدت زمان 60 ثانیه برای بررسی تماس‌های ورودی
  while (millis() - tm < 60000) {
    // بررسی اگر داده‌ای از ماژول GSM در دسترس باشد
    if (Serial.available() > 0) {
      incoming_number = Serial.readString(); // خواندن داده‌های ورودی
    }

    // بررسی اگر داده‌های ورودی نشان‌دهنده تماس ورودی باشد ("RING")
    if (incoming_number.indexOf("RING") >= 0) {
      check_number(incoming_number); // فراخوانی تابع `check_number` برای اعتبارسنجی تماس‌گیرنده
    }
  }
}

void check_number(String data_to_parse) {
  // استخراج شماره تلفن از داده‌های تماس ورودی
  data_to_parse = data_to_parse.substring(data_to_parse.indexOf(":") + 3, data_to_parse.indexOf(",") - 1);

  // بررسی اگر شماره استخراج‌شده با یکی از شماره‌های ذخیره شده در EEPROM تطابق داشته باشد
  if (data_to_parse == read_number(0) || data_to_parse == read_number(20)) {
    Serial.print("ATH\r\n"); // ارسال دستور "ATH" برای قطع تماس
    delay(500); // انتظار کوتاه برای پردازش دستور
    wdt_enable(WDTO_4S); // فعال‌سازی تایمر نگهبان با تایم‌اوت 4 ثانیه برای بازنشانی سیستم
  }
}
