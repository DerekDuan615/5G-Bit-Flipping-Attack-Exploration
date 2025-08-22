# This branch includes the experiments on keystream-based shuffling #

# OpenAirInterface License #

 *  [OAI License Model](http://www.openairinterface.org/?page_id=101)
 *  [OAI License v1.1 on our website](http://www.openairinterface.org/?page_id=698)

It is distributed under **OAI Public License V1.1**.

The license information is distributed under [LICENSE](LICENSE) file in the same directory.

Please see [NOTICE](NOTICE.md) file for third party software that is included in the sources.

# Where to Start #

 *  [General overview of documentation](./doc/README.md)
 *  [The implemented features](./doc/FEATURE_SET.md)
 *  [System Requirements for Using OAI Stack](./doc/system_requirements.md)
 *  [How to build](./doc/BUILD.md)
 *  [How to run the modems](./doc/RUNMODEM.md)

Not all information is available in a central place, and information for
specific sub-systems might be available in the corresponding sub-directories.
To find all READMEs, this command might be handy:

```
find . -iname "readme*"
```

# RAN repository structure #

The OpenAirInterface (OAI) software is composed of the following parts: 

```
openairinterface5g
├── charts
├── ci-scripts        : Meta-scripts used by the OSA CI process. Contains also configuration files used day-to-day by CI.
├── CMakeLists.txt    : Top-level CMakeLists.txt for building
├── cmake_targets     : Build utilities to compile (simulation, emulation and real-time platforms), and generated build files.
├── common            : Some common OAI utilities, some other tools can be found at openair2/UTILS.
├── doc               : Documentation
├── docker            : Dockerfiles to build for Ubuntu and RHEL
├── executables       : Top-level executable source files (gNB, eNB, ...)
├── maketags          : Script to generate emacs tags.
├── nfapi             : (n)FAPI code for MAC-PHY interface
├── openair1          : Layer 1 (3GPP LTE Rel-10/12 PHY, NR Rel-15 PHY)
├── openair2          : Layer 2 (3GPP LTE Rel-10 MAC/RLC/PDCP/RRC/X2AP, LTE Rel-14 M2AP, NR Rel-15+ MAC/RLC/PDCP/SDAP/RRC/X2AP/F1AP/E1AP), E2AP
├── openair3          : Layer 3 (3GPP LTE Rel-10 S1AP/GTP, NR Rel-15 NGAP/GTP)
├── openshift         : OpenShift helm charts for some deployment options of OAI
├── radio             : Drivers for various radios such as USRP, AW2S, RFsim, 7.2 FHI, ...
├── targets           : Some configuration files; only historical relevance, and might be deleted in the future
└── tools             : Tools for use by the developers/ci machines: code analysis and formatting
```

# How to get support from the OAI Community # 

You can ask your question on the [mailing lists](https://gitlab.eurecom.fr/oai/openairinterface5g/-/wikis/MailingList).

Your email should contain below information:

- A clear subject in your email.
- For all the queries there should be [Query\] in the subject of the email and for problems there should be [Problem\].
- In case of a problem, add a small description.
- Do not share any photos unless you want to share a diagram.
- OAI gNB/DU/CU/CU-CP/CU-UP configuration file in `.conf` format only.
- Logs of OAI gNB/DU/CU/CU-CP/CU-UP in `.log` or `.txt` format only.
- In case your question is related to performance, include a small description of the machine (Operating System, Kernel version, CPU, RAM and networking card) and diagram of your testing environment.
- Known/open issues are present on [GitLab](https://gitlab.eurecom.fr/oai/openairinterface5g/-/issues), so keep checking.

Always remember a structured email will help us understand your issues quickly.
