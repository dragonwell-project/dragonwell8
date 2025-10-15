![Dragonwell Logo](https://raw.githubusercontent.com/wiki/dragonwell-project/dragonwell8/images/dragonwell_std_txt_horiz.png)

[Alibaba Dragonwell8 User Guide](https://github.com/dragonwell-project/dragonwell8/wiki/Alibaba-Dragonwell8-User-Guide)

[Alibaba Dragonwell8 Extended Edition Release Notes](https://github.com/dragonwell-project/dragonwell8/wiki/Alibaba-Dragonwell8-Extended-Edition-Release-Notes)

[Alibaba Dragonwell8 Standard Edition Release Notes](https://github.com/dragonwell-project/dragonwell8/wiki/Alibaba-Dragonwell8-Standard-Edition-Release-Notes)

# Introduction

Over the years, Java has proliferated in Alibaba. Many applications are written in Java and many our Java developers have written more than one billion lines of Java code.

Alibaba Dragonwell, as a downstream version of OpenJDK, is the in-house OpenJDK implementation at Alibaba optimized for online e-commerce, financial, logistics applications running on 100,000+ servers. Alibaba Dragonwell is the engine that runs these distributed Java applications in extreme scaling.

The current release supports Linux/x86_64 platform only.

Alibaba Dragonwell is clearly a "friendly fork" under the same licensing terms as the upstream OpenJDK project. Alibaba is committed to collaborate closely with OpenJDK community and intends to bring as many customized features as possible from Alibaba Dragonwell to the upstream.

# Using Alibaba Dragonwell

Alibaba Dragonwell JDK currently supports Linux/x86_64 platform only.

### Installation

##### Option 1, Download and install pre-built Alibaba Dragonwell

* You may download a pre-built Alibaba Dragonwell JDK from its GitHub page:
https://github.com/dragonwell-project/dragonwell8/releases.
* Uncompress the package to the installation directory.

##### Option 2, Install via YUM

Alibaba Dragonwell is officially supported and maintained in Alibaba Cloud Linux 2 (Aliyun Linux 2) YUM repository, and this repo should be also compatible with Aliyun Linux 17.1, Red Hat Enterprise Linux 7 and CentOS 7.

* For users running Alibaba Cloud Linux 2 OS, you should be able to install Alibaba Dragonwell by simply running: `sudo yum install -y java-1.8.0-alibaba-dragonwell`;
* For users running with aforementioned compatible distros, place a new repository file under `/etc/yum.repos.d` (e.g.: `/etc/yum.repos.d/alinux-plus.repo`) with contents as follows, then you should be able to install Alibaba Dragonwell by executing: `sudo yum install -y java-1.8.0-alibaba-dragonwell`:
```
# plus packages provided by Aliyun Linux dev team
[plus]
name=AliYun-2.1903 - Plus - mirrors.aliyun.com
baseurl=http://mirrors.aliyun.com/alinux/2.1903/plus/$basearch/
gpgcheck=1
gpgkey=http://mirrors.aliyun.com/alinux/RPM-GPG-KEY-ALIYUN
```

### Enable Alibaba Dragonwell for Java applications

To enable Alibaba Dragonwell JDK for your application, simply set `JAVA_HOME` to point to the installation directory of Alibaba Dragonwell. If you installed Dragonwell JDK via YUM, follow the instructions prompted from post-install outputs, e.g.:

```
=======================================================================
Alibaba Dragonwell is installed to:
    /opt/alibaba/java-1.8.0-alibaba-dragonwell-8.0.0.212.b04-1.al7
You can set Alibaba Dragonwell as default JDK by exporting the
following ENV VARs:
$ export JAVA_HOME=/opt/alibaba/java-1.8.0-alibaba-dragonwell-8.0.0.212.b04-1.al7
$ export PATH=${JAVA_HOME}/bin:$PATH
=======================================================================
```

# Acknowledgement

Special thanks to those who have made contributions to Alibaba's internal JDK builds.

# Publications

Technologies included in Alibaba Dragonwell have been published in following papers

* ICSE'19：https://2019.icse-conferences.org/event/icse-2019-technical-papers-safecheck-safety-enhancement-of-java-unsafe-api

* ICPE'18: https://dl.acm.org/citation.cfm?id=3186295

* ICSE'18 SEIP  https://www.icse2018.org/event/icse-2018-software-engineering-in-practice-java-performance-troubleshooting-and-optimization-at-alibaba
