### How to use this sample?

**Notice:  this sample is based on esp-idf v5.0.2, make sure that you are using the right esp-idf version.**

Here is my git command when I trying to get esp-idf: *git clone -b release/v5.0 --recursive https://github.com/espressif/esp-idf.git*

---

* Firstly, Since this sample is based upon the *esp-idf-lib*, so, you need to `git clone https://github.com/UncleRus/esp-idf-lib.git`。
* Sencondly, the file *Kconfig.projbuild* has been added to project. We can just using command `idf.py menuconfig` to set our project.
  * Run `idf.py menuconfig`  and go to the option *Climate_terminal02 configuration* 
  * Get all setting done.
  * Return to the main menu of menuconfig  and go to the option *Example Connection Configuration*
  * Filling your wifi ssid and password to the setting. 
  * Press s to save what your setted.
* And this sample should be running correctly.