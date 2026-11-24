"""Beijing Metro — schematic map based on real 2024–2025 network data.

Coverage: Lines 1, 2, 3, 4/Daxing, 5, 6, 7, 8, 9, 10, 13, 14, 15, 16,
         Batong, Changping, Fangshan, Yizhuang, Capital Airport Express,
         Daxing Airport Express.

Coordinate system: schematic — centre ~Tiananmen (0, 0), 1 unit ≈ 1.5 km.

Data source: Beijing Metro official map + Wikipedia station lists (2024–2025).
"""

from main import Station, StationType, Line, MetroNetwork


def _st(net, sid, name, x, y, stype=StationType.UNDERGROUND):
    """Create and register a station. Returns the Station object."""
    if sid in net.stations:
        return net.stations[sid]
    st = Station(sid, name, (x, y), stype)
    net.stations[sid] = st
    return st


def create_beijing_metro() -> MetroNetwork:
    net = MetroNetwork()

    # =====================================================================
    # Stations — each with unique ID.  Transfer stations use the SAME ID
    # across lines for automatic transfer detection.
    # =====================================================================

    # --- Line 1 (Red) — West-East through centre ---
    # Pingguoyuan → Sihui East
    s103 = _st(net, 103, "Pingguoyuan",        -17, 1)
    s102 = _st(net, 102, "Gucheng",             -15.5, 1)
    s101 = _st(net, 101, "Bajiao Amusement Pk", -14, 1)
    s118 = _st(net, 118, "Babaoshan",           -12, 1)
    s117 = _st(net, 117, "Yuquan Lu",           -10, 1.2)
    s116 = _st(net, 116, "Wukesong",            -8.5, 1.3)
    s115 = _st(net, 115, "Wanshou Lu",          -7, 1.2)
    s114 = _st(net, 114, "Gongzhufen",          -5.5, 1)   # X: Line 10
    s113 = _st(net, 113, "Military Museum",     -4.3, 0.5)  # X: Line 9
    s112 = _st(net, 112, "Muxidi",              -3, 0)       # X: Line 16
    s111 = _st(net, 111, "Nanlishi Lu",         -1.8, 0)
    s110 = _st(net, 110, "Fuxingmen",           -1.1, 0)     # X: Line 2
    s109 = _st(net, 109, "Xidan",               -0.2, 0.5)    # X: Line 4
    s108 = _st(net, 108, "Tiananmen West",       0.8, 1.2)
    s107 = _st(net, 107, "Tiananmen East",       2, 1.3)
    s106 = _st(net, 106, "Wangfujing",           3, 1)        # X: Line 8
    s105 = _st(net, 105, "Dongdan",              4, 0.5)      # X: Line 5
    s104 = _st(net, 104, "Jianguomen",           5.5, 0)      # X: Line 2
    s128 = _st(net, 128, "Yong'anli",            6.5, 0)
    s127 = _st(net, 127, "Guomao",               7.5, 0.3)    # X: Line 10
    s126 = _st(net, 126, "Dawang Lu",            8.5, 0)      # X: Line 14
    s125 = _st(net, 125, "Sihui",                10, 0.2)     # X: Batong
    s124 = _st(net, 124, "Sihui East",           11, 0.5)     # X: Batong

    # --- Batong Line (Red, through service with Line 1) ---
    s201 = _st(net, 201, "Gaobeidian",           12.5, 0.5)
    s202 = _st(net, 202, "Comm. Univ. of China", 14, 0.8)
    s203 = _st(net, 203, "Shuangqiao",           15, 0.6)
    s204 = _st(net, 204, "Guanzhuang",           16, 0.3)
    s205 = _st(net, 205, "Baliqiao",             17, -0.2)
    s206 = _st(net, 206, "Tongzhou Beiyuan",     18.5, -0.5)
    s207 = _st(net, 207, "Guoyuan",              20, -0.3)
    s208 = _st(net, 208, "Jiukeshu",             21, 0)
    s209 = _st(net, 209, "Liyuan",               22, 0.3)
    s210 = _st(net, 210, "Linheli",              23, 0.2)
    s211 = _st(net, 211, "Tuqiao",               24, -0.2)
    s212 = _st(net, 212, "Huazhuang",            25.5, -0.5)  # X: Line 7
    s213 = _st(net, 213, "Universal Resort",     27, -0.3)    # X: Line 7

    # --- Line 2 (Blue) — Loop line, clockwise from Xizhimen ---
    s2013 = _st(net, 2013, "Xizhimen",          -2.5, 4)     # X: L4, L13
    s221 = _st(net, 221, "Jishuitan",            -1.2, 4.5)    # X: L19
    s222 = _st(net, 222, "Gulou Dajie",          1, 4.8)       # X: L8
    s223 = _st(net, 223, "Andingmen",            2.2, 4.2)
    s224 = _st(net, 224, "Yonghegong",           3.3, 3.8)     # X: L5
    s225 = _st(net, 225, "Dongzhimen",           4.3, 3.2)     # X: L13, Airport
    s226 = _st(net, 226, "Dongsi Shitiao",       4.8, 1.8)     # X: L3
    s227 = _st(net, 227, "Chaoyangmen",          5.5, 0.8)     # X: L6
    s228 = _st(net, 228, "Jianguomen",           5.5, 0)       # X: L1
    s229 = _st(net, 229, "Beijing Railway Stn",  4.5, -1.5)
    s230 = _st(net, 230, "Chongwenmen",          3.5, -2)      # X: L5
    s231 = _st(net, 231, "Qianmen",              2, -2.5)      # X: L8
    s232 = _st(net, 232, "Hepingmen",            1, -3)
    s233 = _st(net, 233, "Xuanwumen",            -0.5, -3)      # X: L4
    s234 = _st(net, 234, "Changchunjie",         -1.8, -2.5)
    s235 = _st(net, 235, "Fuxingmen",            -1.1, 0)       # X: L1
    s236 = _st(net, 236, "Fuchengmen",           -2, 1.5)
    s237 = _st(net, 237, "Chegongzhuang",        -2.5, 2.5)     # X: L6

    # --- Line 3 (Rose Red) — 2024 new line ---
    s301 = _st(net, 301, "Dongsi Shitiao",       4.8, 1.8)      # X: L2
    s302 = _st(net, 302, "Workers' Stadium",     5.5, 2.5)      # X: L17
    s303 = _st(net, 303, "Tuanjiehu",            6.2, 3)        # X: L10
    s304 = _st(net, 304, "Chaoyang Park",        7, 3.8)        # X: L14
    s305 = _st(net, 305, "Shifoying",            7.8, 4.5)
    s306 = _st(net, 306, "Chaoyang Railway Stn", 8.5, 5)
    s307 = _st(net, 307, "Yaojiayuan",           9.3, 5.5)
    s308 = _st(net, 308, "Dongbanan",            10, 6)
    s309 = _st(net, 309, "Dongba",               11, 6.5)
    s310 = _st(net, 310, "Dongbabei",            12, 7)         # X: L12

    # --- Line 4 / Daxing Line (Teal) — North-South ---
    s411 = _st(net, 411, "Anheqiao North",       -1, 16)
    s410 = _st(net, 410, "Beigongmen",           -1.2, 14.5)
    s409 = _st(net, 409, "Xi Yuan",              -1, 13)        # X: L16
    s408 = _st(net, 408, "Yuanmingyuan Park",    -0.8, 11.5)
    s407 = _st(net, 407, "Peking Univ. East",    -0.5, 10)
    s406 = _st(net, 406, "Zhongguancun",         -0.3, 8.5)
    s405 = _st(net, 405, "Haidian Huangzhuang",  -0.2, 7.5)     # X: L10
    s404 = _st(net, 404, "Renmin Univ.",         -0.2, 6.5)     # X: L12
    s403 = _st(net, 403, "Weigongcun",           -0.3, 5.5)
    s402 = _st(net, 402, "National Library",     -0.5, 4.5)     # X: L9, L16
    s401 = _st(net, 401, "Beijing Zoo",          -0.8, 4)
    s400 = _st(net, 400, "Xizhimen",             -2.5, 4)       # X: L2, L13
    s412 = _st(net, 412, "Xinjiekou",            -1.8, 3)
    s413 = _st(net, 413, "Ping'anli",            -0.8, 2.8)     # X: L6, L19
    s414 = _st(net, 414, "Xisi",                 -0.5, 1.5)
    s415 = _st(net, 415, "Lingjing Hutong",      -0.3, 1)
    s416 = _st(net, 416, "Xidan",                -0.2, 0.5)     # X: L1
    s417 = _st(net, 417, "Xuanwumen",            -0.5, -3)      # X: L2
    s418 = _st(net, 418, "Caishikou",            0, -4)         # X: L7
    s419 = _st(net, 419, "Taoranting",           0.5, -5)
    s420 = _st(net, 420, "Beijing South Stn",    1, -5.8)      # X: L14
    s421 = _st(net, 421, "Majiapu",              1, -7)
    s422 = _st(net, 422, "Jiaomen West",         1, -8)        # X: L10
    s423 = _st(net, 423, "Gongyi Xiqiao",        1, -9)
    s424 = _st(net, 424, "Xingong",              1, -10.5)      # X: L19
    s425 = _st(net, 425, "Xihongmen",            1, -12)
    s426 = _st(net, 426, "Gaomidian North",      1, -13.5)
    s427 = _st(net, 427, "Gaomidian South",      1, -15)
    s428 = _st(net, 428, "Zaoyuan",              1, -16.5)
    s429 = _st(net, 429, "Qingyuan Lu",          1, -18)
    s430 = _st(net, 430, "Huangcun Xidajie",     1, -19.5)
    s431 = _st(net, 431, "Huangcun Railway Stn", 1, -21)
    s432 = _st(net, 432, "Yihezhuang",           1, -22.5)
    s433 = _st(net, 433, "Biomedical Base",      1, -24)
    s434 = _st(net, 434, "Tiangong Yuan",        1, -25.5)

    # --- Line 5 (Purple) — North-South ---
    s501 = _st(net, 501, "Tiantongyuan North",   6.5, 16)
    s502 = _st(net, 502, "Tiantongyuan",         6.5, 14.5)
    s503 = _st(net, 503, "Tiantongyuan South",   6.3, 13)
    s504 = _st(net, 504, "Lishuiqiao",           6, 11.5)       # X: L13
    s505 = _st(net, 505, "Lishuiqiao South",     5.8, 10)
    s506 = _st(net, 506, "Beiyuanlu North",      5.5, 8.5)
    s507 = _st(net, 507, "Datunlu East",         5.3, 7.5)      # X: L15
    s508 = _st(net, 508, "Huixin Xijie Beikou",  5, 6.5)
    s509 = _st(net, 509, "Huixin Xijie Nankou",  4.8, 5.5)     # X: L10
    s510 = _st(net, 510, "Heping Xiqiao",        4.5, 4.8)     # X: L12
    s511 = _st(net, 511, "Hepingli Beijie",      4.2, 4.5)
    s512 = _st(net, 512, "Yonghegong",           3.3, 3.8)     # X: L2
    s513 = _st(net, 513, "Beixinqiao",           3.5, 3)       # X: Capital Airport
    s514 = _st(net, 514, "Zhangzizhong Lu",      3.8, 2)
    s515 = _st(net, 515, "Dongsi",               4, 1)         # X: L6
    s516 = _st(net, 516, "Dengshikou",           4, 0.7)
    s517 = _st(net, 517, "Dongdan",              4, 0.5)       # X: L1
    s518 = _st(net, 518, "Chongwenmen",          3.5, -2)      # X: L2
    s519 = _st(net, 519, "Ciqikou",              3.8, -3)      # X: L7
    s520 = _st(net, 520, "Temple of Heaven East",4.5, -3.5)
    s521 = _st(net, 521, "Puhuangyu",            5, -4)        # X: L14
    s522 = _st(net, 522, "Liujiayao",            5.5, -5)
    s523 = _st(net, 523, "Songjiazhuang",        6, -6)        # X: L10, Yizhuang

    # --- Line 6 (Earth Yellow) — East-West ---
    s601 = _st(net, 601, "Jin'anqiao",           -16.5, 5)     # X: L11, S1
    s602 = _st(net, 602, "Pingguoyuan",          -17, 1)       # X: L1, S1
    s603 = _st(net, 603, "Yangzhuang",           -14.5, 3.5)
    s604 = _st(net, 604, "Xihuangcun",           -12.5, 4)
    s605 = _st(net, 605, "Liaogongzhuang",       -10.5, 4.5)
    s606 = _st(net, 606, "Tiancun",              -8.5, 5)
    s607 = _st(net, 607, "Haidian Wuluju",       -7, 5.5)     # X: L16 (planned)
    s608 = _st(net, 608, "Cishousi",             -5.5, 6)      # X: L10
    s609 = _st(net, 609, "Huayuanqiao",          -4, 6.2)
    s610 = _st(net, 610, "Baishiqiao South",     -2.5, 6)      # X: L9
    s611 = _st(net, 611, "Erligou",              -1.5, 5.5)     # X: L16
    s612 = _st(net, 612, "Chegongzhuang West",   -2.8, 3.5)
    s613 = _st(net, 613, "Chegongzhuang",        -2.5, 2.5)     # X: L2
    s614 = _st(net, 614, "Ping'anli",            -0.8, 2.8)     # X: L4, L19
    s615 = _st(net, 615, "Beihai North",         0.2, 3)
    s616 = _st(net, 616, "Nanluoguxiang",        1.5, 3.5)     # X: L8
    s617 = _st(net, 617, "Dongsi",               4, 1)         # X: L5
    s618 = _st(net, 618, "Chaoyangmen",          5.5, 0.8)     # X: L2
    s619 = _st(net, 619, "Dongdaqiao",           6.5, 1.5)     # X: L17
    s620 = _st(net, 620, "Hujialou",             7.5, 2.5)     # X: L10
    s621 = _st(net, 621, "Jintai Lu",            8.5, 2)       # X: L14
    s622 = _st(net, 622, "Shilipu",              10, 2.5)
    s623 = _st(net, 623, "Qingnian Lu",          11.5, 3)
    s624 = _st(net, 624, "Dalianpo",             13, 3.5)
    s625 = _st(net, 625, "Huangqu",              14.5, 4)
    s626 = _st(net, 626, "Changying",            16, 4.5)
    s627 = _st(net, 627, "Caofang",              17.5, 5)
    s628 = _st(net, 628, "Wuzi Xueyuan Lu",      19, 5.5)
    s629 = _st(net, 629, "Tongzhou Beiguan",     20.5, 5)
    s630 = _st(net, 630, "Beiyunhe West",        22, 5)
    s631 = _st(net, 631, "Beiyunhe East",        23.5, 5)
    s632 = _st(net, 632, "Haojiafu",             25, 5.5)
    s633 = _st(net, 633, "Dongxiayuan",          26.5, 6)
    s634 = _st(net, 634, "Lucheng",              28, 6.5)

    # --- Line 7 (Light Orange) — East-West south of centre ---
    s701 = _st(net, 701, "Beijing West Stn",     -4.5, -4)      # X: L9
    s702 = _st(net, 702, "Wanzi",                -3.5, -3.5)
    s703 = _st(net, 703, "Daguanying",           -2.5, -3.2)     # X: L16
    s704 = _st(net, 704, "Guang'anmen Nei",      -1.5, -3)
    s705 = _st(net, 705, "Caishikou",            0, -4)         # X: L4
    s706 = _st(net, 706, "Hufangqiao",           1.2, -3.8)
    s707 = _st(net, 707, "Zhushikou",            2, -3.5)       # X: L8
    s708 = _st(net, 708, "Qiaowan",              3, -3.2)
    s709 = _st(net, 709, "Ciqikou",              3.8, -3)       # X: L5
    s710 = _st(net, 710, "Guangqumen Nei",       4.5, -3.2)
    s711 = _st(net, 711, "Guangqumen Wai",       5.5, -3.5)
    s712 = _st(net, 712, "Shuangjing",           6.5, -4)       # X: L10
    s713 = _st(net, 713, "Jiulongshan",          7.5, -4)       # X: L14
    s714 = _st(net, 714, "Dajiaoting",           9, -4.5)
    s715 = _st(net, 715, "Baiziwan",             10.5, -4.8)
    s716 = _st(net, 716, "Huagong",              12, -5)
    s717 = _st(net, 717, "Nanlouzizhuang",       13.5, -5.2)
    s718 = _st(net, 718, "Happy Valley",         15, -5)
    s719 = _st(net, 719, "Fatou",                16.5, -4.8)
    s720 = _st(net, 720, "Shuanghe",             18, -4.5)
    s721 = _st(net, 721, "Jiaohuachang",         19.5, -4.3)
    s722 = _st(net, 722, "Huangchang",           21, -4)
    s723 = _st(net, 723, "Langxinzhuang",        22.5, -3.8)
    s724 = _st(net, 724, "Heizhuanghu",          24, -3.5)
    s725 = _st(net, 725, "Wansheng West",        25, -3.2)
    s726 = _st(net, 726, "Wansheng East",        26, -3)
    s727 = _st(net, 727, "Qunfang",              26.5, -2.5)
    s728 = _st(net, 728, "Gaoloujin",            26.8, -1.5)
    s729 = _st(net, 729, "Huazhuang",            25.5, -0.5)     # X: Batong
    s730 = _st(net, 730, "Universal Resort",     27, -0.3)       # X: Batong

    # --- Line 8 (Green) — North-South through centre ---
    s801 = _st(net, 801, "Zhuxinzhuang",         3.5, 12)        # X: Changping
    s802 = _st(net, 802, "Yuzhi Lu",             3.8, 11)
    s803 = _st(net, 803, "Pingxifu",             4.2, 10)
    s804 = _st(net, 804, "Huilongguan Dongdajie",4.5, 9)
    s805 = _st(net, 805, "Huoying",              4.5, 8)         # X: L13
    s806 = _st(net, 806, "Yuxin",                4.2, 7)
    s807 = _st(net, 807, "Xixiaokou",            4, 6.5)
    s808 = _st(net, 808, "Yongtaizhuang",        3.8, 6)
    s809 = _st(net, 809, "Lincui Qiao",          3.5, 5.5)
    s810 = _st(net, 810, "Forest Park S. Gate",  3.2, 5)
    s811 = _st(net, 811, "Olympic Park",         3.5, 4.5)       # X: L15
    s812 = _st(net, 812, "Olympic Sports Ctr",   3.3, 4)
    s813 = _st(net, 813, "Beitucheng",           3, 3.3)         # X: L10
    s814 = _st(net, 814, "Anhua Qiao",           2.8, 2.8)       # X: L12
    s815 = _st(net, 815, "Andeli Beijie",        2.5, 2.3)
    s816 = _st(net, 816, "Gulou Dajie",          1, 4.8)         # X: L2
    s817 = _st(net, 817, "Shichahai",            1.5, 4)
    s818 = _st(net, 818, "Nanluoguxiang",        1.5, 3.5)       # X: L6
    s819 = _st(net, 819, "Nat. Art Museum",      2, 2.5)
    s820 = _st(net, 820, "Jinyu Hutong",         2.5, 2)
    s821 = _st(net, 821, "Wangfujing",           3, 1)           # X: L1
    s822 = _st(net, 822, "Qianmen",              2, -2.5)         # X: L2
    s823 = _st(net, 823, "Zhushikou",            2, -3.5)         # X: L7
    s824 = _st(net, 824, "Tianqiao",             2.5, -4.5)
    s825 = _st(net, 825, "Yongdingmen Wai",      3, -5.5)         # X: L14
    s826 = _st(net, 826, "Muxiyuan",             3.5, -6.5)
    s827 = _st(net, 827, "Haihutun",             4, -7.5)
    s828 = _st(net, 828, "Dahongmen",            4.5, -8.5)      # X: L10
    s829 = _st(net, 829, "Dahongmen South",      4.8, -9.5)
    s830 = _st(net, 830, "Heyi",                 5, -10.5)
    s831 = _st(net, 831, "Donggaodi",            5.5, -11.5)
    s832 = _st(net, 832, "Huojian Wanyuan",      6, -12.5)
    s833 = _st(net, 833, "Wufutang",             6.5, -13.5)
    s834 = _st(net, 834, "Demao",                7, -14.5)
    s835 = _st(net, 835, "Yinghai",              7.5, -15.5)

    # --- Line 9 (Light Green) — North-South west side ---
    s901 = _st(net, 901, "National Library",    -0.5, 4.5)      # X: L4, L16
    s902 = _st(net, 902, "Baishiqiao South",    -2.5, 6)        # X: L6
    s903 = _st(net, 903, "Baiduizi",            -3, 1.5)
    s904 = _st(net, 904, "Military Museum",     -4.3, 0.5)      # X: L1
    s905 = _st(net, 905, "Beijing West Stn",    -4.5, -4)       # X: L7
    s906 = _st(net, 906, "Liuliqiao East",      -4.2, -5)
    s907 = _st(net, 907, "Liuliqiao",           -4, -6)         # X: L10
    s908 = _st(net, 908, "Qilizhuang",          -3.5, -7)       # X: L14
    s909 = _st(net, 909, "Fengtai Dongdajie",   -3, -8)
    s910 = _st(net, 910, "Fengtai Nanlu",       -2.5, -9)       # X: L16
    s911 = _st(net, 911, "Keyi Lu",             -2, -10)
    s912 = _st(net, 912, "Fengtai Science Park",-1.5, -11)
    s913 = _st(net, 913, "Guogongzhuang",       -1, -12)        # X: Fangshan

    # --- Line 10 (Sky Blue) — Outer loop ---
    s1001 = _st(net, 1001, "Bagou",              -8, 11)
    s1002 = _st(net, 1002, "Suzhou Jie",         -5, 10.5)       # X: L16
    s1003 = _st(net, 1003, "Haidian Huangzhuang",-0.2, 7.5)      # X: L4
    s1004 = _st(net, 1004, "Zhichunli",           1, 7.5)
    s1005 = _st(net, 1005, "Zhichun Lu",          2, 7.3)        # X: L13
    s1006 = _st(net, 1006, "Xitucheng",           2.5, 6.8)      # X: Changping
    s1007 = _st(net, 1007, "Mudanyuan",           2, 6)          # X: L19
    s1008 = _st(net, 1008, "Jiandemen",           2.5, 5.5)
    s1009 = _st(net, 1009, "Beitucheng",          3, 3.3)        # X: L8
    s1010 = _st(net, 1010, "Anzhenmen",           3.5, 3.8)
    s1011 = _st(net, 1011, "Huixin Xijie Nankou", 4.8, 5.5)     # X: L5
    s1012 = _st(net, 1012, "Shaoyaoju",           6, 5)          # X: L13
    s1013 = _st(net, 1013, "Taiyanggong",         7, 5)          # X: L17
    s1014 = _st(net, 1014, "Sanyuanqiao",         8, 4.5)        # X: Capital Airport, L12
    s1015 = _st(net, 1015, "Liangmaqiao",         8.5, 3.8)
    s1016 = _st(net, 1016, "Agri. Exhibition",    8.5, 3.2)
    s1017 = _st(net, 1017, "Tuanjiehu",           6.2, 3)        # X: L3
    s1018 = _st(net, 1018, "Hujialou",            7.5, 2.5)      # X: L6
    s1019 = _st(net, 1019, "Jintai Xizhao",       8, 1.5)
    s1020 = _st(net, 1020, "Guomao",              7.5, 0.3)      # X: L1
    s1021 = _st(net, 1021, "Shuangjing",          6.5, -4)       # X: L7
    s1022 = _st(net, 1022, "Jinsong",             7.5, -5)
    s1023 = _st(net, 1023, "Panjiayuan",          8.5, -5.5)
    s1024 = _st(net, 1024, "Shilihe",             9.5, -6)       # X: L14, L17
    s1025 = _st(net, 1025, "Fenzhongsi",          9, -7)
    s1026 = _st(net, 1026, "Chengshousi",         8.5, -7.5)
    s1027 = _st(net, 1027, "Songjiazhuang",       6, -6)         # X: L5, Yizhuang
    s1028 = _st(net, 1028, "Shiliuzhuang",        5, -7)
    s1029 = _st(net, 1029, "Dahongmen",           4.5, -8.5)     # X: L8
    s1030 = _st(net, 1030, "Jiaomen East",        3.5, -8.5)
    s1031 = _st(net, 1031, "Jiaomen West",        1, -8)         # X: L4
    s1032 = _st(net, 1032, "Caoqiao",             0, -9)         # X: L19, Daxing Airport
    s1033 = _st(net, 1033, "Jijiamiao",          -2, -8.5)
    s1034 = _st(net, 1034, "Cap. Univ. Econ.",   -3, -7.5)       # X: Fangshan
    s1035 = _st(net, 1035, "Fengtai Railway Stn",-3.5, -6.5)     # X: L16
    s1036 = _st(net, 1036, "Niwa",               -5, -5)
    s1037 = _st(net, 1037, "Xiju",               -5.5, -6)       # X: L14
    s1038 = _st(net, 1038, "Liuliqiao",          -4, -6)         # X: L9
    s1039 = _st(net, 1039, "Lianhuaqiao",        -5, -2.5)
    s1040 = _st(net, 1040, "Gongzhufen",         -5.5, 1)        # X: L1
    s1041 = _st(net, 1041, "Xidiaoyutai",        -5, 2.5)
    s1042 = _st(net, 1042, "Cishousi",           -5.5, 6)        # X: L6
    s1043 = _st(net, 1043, "Chedaogou",          -6.5, 7)
    s1044 = _st(net, 1044, "Changchunqiao",      -7, 8.5)        # X: L12
    s1045 = _st(net, 1045, "Huoqiying",          -7.5, 10)

    # --- Line 13 (Yellow) — Northern arc ---
    s1301 = _st(net, 1301, "Xizhimen",           -2.5, 4)        # X: L2, L4
    s1302 = _st(net, 1302, "Dazhongsi",          -1, 5.5)
    s1303 = _st(net, 1303, "Zhichun Lu",          2, 7.3)        # X: L10
    s1304 = _st(net, 1304, "Wudaokou",            3, 8)
    s1305 = _st(net, 1305, "Shangdi",             4, 9.5)
    s1306 = _st(net, 1306, "Qinghe Railway Stn",  5, 10.5)
    s1307 = _st(net, 1307, "Xi'erqi",             5.5, 11.5)     # X: Changping
    s1308 = _st(net, 1308, "Longze",              6, 12.5)
    s1309 = _st(net, 1309, "Huilongguan",         5.5, 13)
    s1310 = _st(net, 1310, "Huoying",             4.5, 8)        # X: L8
    s1311 = _st(net, 1311, "Lishuiqiao",          6, 11.5)       # X: L5
    s1312 = _st(net, 1312, "Beiyuan",             7, 9)
    s1313 = _st(net, 1313, "Wangjing West",       9, 7.5)        # X: L15, L17
    s1314 = _st(net, 1314, "Shaoyaoju",           6, 5)          # X: L10
    s1315 = _st(net, 1315, "Guangximen",          5.5, 3.5)      # X: L12
    s1316 = _st(net, 1316, "Liufang",             5, 3.2)
    s1317 = _st(net, 1317, "Dongzhimen",          4.3, 3.2)      # X: L2, Capital Airport

    # --- Line 14 (Light Pink) — L-shaped ---
    s1401 = _st(net, 1401, "Zhangguozhuang",     -9, -11)
    s1402 = _st(net, 1402, "Garden Expo Park",   -8, -10)
    s1403 = _st(net, 1403, "Dawayao",            -7.5, -9)
    s1404 = _st(net, 1404, "Guozhuangzi",        -6.5, -8.5)
    s1405 = _st(net, 1405, "Dajing",             -6, -7.5)
    s1406 = _st(net, 1406, "Qilizhuang",         -3.5, -7)       # X: L9
    s1407 = _st(net, 1407, "Xiju",               -5.5, -6)       # X: L10
    s1408 = _st(net, 1408, "Dongguantou",        -4, -6)
    s1409 = _st(net, 1409, "Lize Shangwuqu",     -3, -5.5)      # X: L16
    s1410 = _st(net, 1410, "Caihuying",          -2, -5.5)
    s1411 = _st(net, 1411, "Xitieying",          -1, -5.5)
    s1412 = _st(net, 1412, "Jingfengmen",         0, -5.8)      # X: L19
    s1413 = _st(net, 1413, "Beijing South Stn",   1, -5.8)      # X: L4
    s1414 = _st(net, 1414, "Yongdingmen Wai",     3, -5.5)      # X: L8
    s1415 = _st(net, 1415, "Jingtai",             4, -5)
    s1416 = _st(net, 1416, "Puhuangyu",           5, -4)         # X: L5
    s1417 = _st(net, 1417, "Fangzhuang",          6, -4.5)
    s1418 = _st(net, 1418, "Shilihe",             9.5, -6)       # X: L10, L17
    s1419 = _st(net, 1419, "BJUT West Gate",      7, -5.5)
    s1420 = _st(net, 1420, "Pingleyuan",          7.5, -5)
    s1421 = _st(net, 1421, "Jiulongshan",         7.5, -4)       # X: L7
    s1422 = _st(net, 1422, "Dawang Lu",           8.5, 0)        # X: L1
    s1423 = _st(net, 1423, "Jintai Lu",           8.5, 2)        # X: L6
    s1424 = _st(net, 1424, "Chaoyang Park",       7, 3.8)        # X: L3
    s1425 = _st(net, 1425, "Zaoying",             7.5, 5)
    s1426 = _st(net, 1426, "Dongfeng Beiqiao",    8, 6)
    s1427 = _st(net, 1427, "Jiangtai",            8.5, 7)
    s1428 = _st(net, 1428, "Wangjing South",      9, 8)
    s1429 = _st(net, 1429, "Futong",              9.5, 8.5)
    s1430 = _st(net, 1430, "Wangjing",            10, 8)         # X: L15
    s1431 = _st(net, 1431, "Donghuqu",            10.5, 9)
    s1432 = _st(net, 1432, "Laiguangying",        11, 10)
    s1433 = _st(net, 1433, "Shan'gezhuang",       11.5, 11)

    # --- Line 15 (Violet) — Far north ---
    s1501 = _st(net, 1501, "Qinghua Donglu Xikou",-1.5, 10.5)
    s1502 = _st(net, 1502, "Liudaokou",            0.5, 10.5)     # X: Changping
    s1503 = _st(net, 1503, "Beishatan",            2, 9.5)
    s1504 = _st(net, 1504, "Olympic Park",         3.5, 4.5)      # X: L8
    s1505 = _st(net, 1505, "Anli Lu",              4, 9)
    s1506 = _st(net, 1506, "Datunlu East",         5.3, 7.5)      # X: L5
    s1507 = _st(net, 1507, "Guanzhuang",           6.5, 7.5)
    s1508 = _st(net, 1508, "Wangjing West",        9, 7.5)        # X: L13
    s1509 = _st(net, 1509, "Wangjing",             10, 8)         # X: L14
    s1510 = _st(net, 1510, "Wangjing East",        11, 8.5)
    s1511 = _st(net, 1511, "Cuigezhuang",          12.5, 8.5)
    s1512 = _st(net, 1512, "Maquanying",           14, 9)
    s1513 = _st(net, 1513, "Sunhe",                16, 9.5)
    s1514 = _st(net, 1514, "China Intl. Exhib.",   18, 9)
    s1515 = _st(net, 1515, "Hualikan",             20, 9.5)
    s1516 = _st(net, 1516, "Houshayu",             22, 10)
    s1517 = _st(net, 1517, "Nanfaxin",             24, 10.5)
    s1518 = _st(net, 1518, "Shimen",               25.5, 11.5)
    s1519 = _st(net, 1519, "Shunyi",               27, 11)
    s1520 = _st(net, 1520, "Fengbo",               28.5, 11.5)

    # --- Line 16 (Green) — Far west / south ---
    s1601 = _st(net, 1601, "Bei'anhe",            -8, 19)
    s1602 = _st(net, 1602, "Wenyang Lu",          -7.5, 17.5)
    s1603 = _st(net, 1603, "Daoxianghu Lu",       -7, 16)
    s1604 = _st(net, 1604, "Tundian",             -6.5, 14.5)
    s1605 = _st(net, 1605, "Yongfeng",            -6, 13)
    s1606 = _st(net, 1606, "Yongfeng South",      -5.5, 11.5)
    s1607 = _st(net, 1607, "Xibeiwang",           -5, 10.5)
    s1608 = _st(net, 1608, "Malianwa",            -4.5, 9.5)
    s1609 = _st(net, 1609, "Nongda Nanlu",        -4, 8.5)
    s1610 = _st(net, 1610, "Xi Yuan",             -1, 13)         # X: L4
    s1611 = _st(net, 1611, "Wanquanhe Qiao",      -3, 12)
    s1612 = _st(net, 1612, "Suzhou Jie",          -5, 10.5)       # X: L10
    s1613 = _st(net, 1613, "Suzhouqiao",          -4, 9)
    s1614 = _st(net, 1614, "Wanshousi",           -3.5, 7.5)
    s1615 = _st(net, 1615, "National Library",    -0.5, 4.5)      # X: L4, L9
    s1616 = _st(net, 1616, "Erligou",             -1.5, 5.5)      # X: L6
    s1617 = _st(net, 1617, "Ganjia Kou",          -2, 3)
    s1618 = _st(net, 1618, "Yuyuantan E. Gate",   -2.5, 1.5)
    s1619 = _st(net, 1619, "Muxidi",              -3, 0)           # X: L1
    s1620 = _st(net, 1620, "Daguanying",          -2.5, -3.2)      # X: L7
    s1621 = _st(net, 1621, "Honglian Nanlu",      -2.5, -4)
    s1622 = _st(net, 1622, "Lize Shangwuqu",      -3, -5.5)        # X: L14
    s1623 = _st(net, 1623, "Dongguantou South",   -3.5, -6)        # X: Fangshan
    s1624 = _st(net, 1624, "Fengtai Railway Stn", -3.5, -6.5)      # X: L10
    s1625 = _st(net, 1625, "Fengtai Nanlu",       -2.5, -9)        # X: L9
    s1626 = _st(net, 1626, "Fufengqiao",          -3, -10.5)
    s1627 = _st(net, 1627, "Kandan",              -3.5, -12)
    s1628 = _st(net, 1628, "Yushuzhuang",         -4, -13.5)
    s1629 = _st(net, 1629, "Hongtaizhuang",       -4.5, -15)
    s1630 = _st(net, 1630, "Wanpingcheng",        -5, -16.5)

    # --- Changping Line (Pink) — North ---
    s1701 = _st(net, 1701, "Changping Xishankou",  4, 24)
    s1702 = _st(net, 1702, "Ming Tombs",           4, 22)
    s1703 = _st(net, 1703, "Changping",            4.5, 20)
    s1704 = _st(net, 1704, "Changping Dongguan",   5, 18)
    s1705 = _st(net, 1705, "Beishaowa",            5.5, 17)
    s1706 = _st(net, 1706, "Nanshao",              5.5, 16)
    s1707 = _st(net, 1707, "Shahe Univ. Park",     5, 15)
    s1708 = _st(net, 1708, "Shahe",                5, 14)
    s1709 = _st(net, 1709, "Gonghuacheng",         4.5, 13)
    s1710 = _st(net, 1710, "Zhuxinzhuang",         3.5, 12)       # X: L8
    s1711 = _st(net, 1711, "Life Science Park",    4, 11.5)
    s1712 = _st(net, 1712, "Xi'erqi",              5.5, 11.5)     # X: L13
    s1713 = _st(net, 1713, "Qinghe Railway Stn",   5, 10.5)       # X: L13 (planned)
    s1714 = _st(net, 1714, "Zhufangbei",           3.5, 10)
    s1715 = _st(net, 1715, "Qinghe Xiaoyingqiao",  2.5, 10)
    s1716 = _st(net, 1716, "Xueyuanqiao",          1.5, 10.5)
    s1717 = _st(net, 1717, "Liudaokou",            0.5, 10.5)     # X: L15
    s1718 = _st(net, 1718, "Xuezhiyuan",           0, 9)
    s1719 = _st(net, 1719, "Xitucheng",            2.5, 6.8)      # X: L10
    s1720 = _st(net, 1720, "Jimenqiao",            1, 6.5)        # X: L12

    # --- Fangshan Line (Orange) — Southwest ---
    s1801 = _st(net, 1801, "Dongguantou South",   -3.5, -6)       # X: L16
    s1802 = _st(net, 1802, "Cap. Univ. Econ.",    -3, -7.5)       # X: L10
    s1803 = _st(net, 1803, "Huaxiang Dongqiao",   -2.5, -9)
    s1804 = _st(net, 1804, "Baipenyao",           -2, -10.5)
    s1805 = _st(net, 1805, "Guogongzhuang",       -1, -12)        # X: L9
    s1806 = _st(net, 1806, "Dabaotai",            -1.5, -13.5)
    s1807 = _st(net, 1807, "Daotian",             -2, -15)
    s1808 = _st(net, 1808, "Changyang",           -2.5, -16.5)
    s1809 = _st(net, 1809, "Libafang",            -3, -18)
    s1810 = _st(net, 1810, "Guangyangcheng",      -3, -19.5)
    s1811 = _st(net, 1811, "Liangxiang Univ. N.", -3.5, -21)
    s1812 = _st(net, 1812, "Liangxiang Univ.",    -3.5, -22.5)
    s1813 = _st(net, 1813, "Liangxiang Univ. W.", -3.5, -24)
    s1814 = _st(net, 1814, "Liangxiang Nanguan",  -3, -25.5)
    s1815 = _st(net, 1815, "Suzhuang",            -3.5, -27)
    s1816 = _st(net, 1816, "Yancun East",         -4, -28.5)

    # --- Yizhuang Line (Peach Pink) — Southeast ---
    s1901 = _st(net, 1901, "Songjiazhuang",        6, -6)         # X: L5, L10
    s1902 = _st(net, 1902, "Xiaocun",              7, -7)
    s1903 = _st(net, 1903, "Xiaohongmen",          8, -8)
    s1904 = _st(net, 1904, "Jiugong",              9, -9)
    s1905 = _st(net, 1905, "Yizhuangqiao",         10, -10)
    s1906 = _st(net, 1906, "Yizhuang Culture Pk.", 11, -11)
    s1907 = _st(net, 1907, "Wanyuanjie",           12, -12)
    s1908 = _st(net, 1908, "Rongjing Dongjie",     13, -12.5)
    s1909 = _st(net, 1909, "Rongchang Dongjie",    14, -13)
    s1910 = _st(net, 1910, "Tongji Nanlu",         15, -13.5)
    s1911 = _st(net, 1911, "Jinghai Lu",           16, -14)
    s1912 = _st(net, 1912, "Ciqunan",              17, -14.5)
    s1913 = _st(net, 1913, "Ciqu",                 18, -15)
    s1914 = _st(net, 1914, "Yizhuang Railway Stn", 19, -15.5)

    # --- Capital Airport Express (Silver) ---
    s2001 = _st(net, 2001, "Beixinqiao",          3.5, 3)         # X: L5
    s2002 = _st(net, 2002, "Dongzhimen",          4.3, 3.2)       # X: L2, L13
    s2003 = _st(net, 2003, "Sanyuanqiao",         8, 4.5)         # X: L10, L12
    s2004 = _st(net, 2004, "Terminal 3",          18, 9)
    s2005 = _st(net, 2005, "Terminal 2",          16, 10.5)

    # --- Daxing Airport Express (Dark Blue) ---
    s2101 = _st(net, 2101, "Caoqiao",             0, -9)          # X: L10, L19
    s2102 = _st(net, 2102, "Daxing Xincheng",     2, -18)
    s2103 = _st(net, 2103, "Daxing Airport",      4, -28)

    # =====================================================================
    # Lines
    # =====================================================================

    # --- Line 1 (Red) ---
    net.add_line(Line(1, "Line 1", [
        s103, s102, s101, s118, s117, s116, s115, s114, s113, s112,
        s111, s110, s109, s108, s107, s106, s105, s104, s128, s127,
        s126, s125, s124,
    ], max_speed=80, color=(200, 40, 40)))

    # --- Batong Line (Red) ---
    net.add_line(Line(2, "Batong Line", [
        s125, s201, s202, s203, s204, s205, s206, s207, s208,
        s209, s210, s211, s212, s213,
    ], max_speed=80, color=(200, 40, 40)))

    # --- Line 2 (Blue, Loop) ---
    net.add_line(Line(3, "Line 2", [
        s2013, s221, s222, s223, s224, s225, s226, s227, s228,
        s229, s230, s231, s232, s233, s234, s235, s236, s237, s2013,
    ], max_speed=80, color=(30, 100, 200), ring_label_station_id=2013))

    # --- Line 3 (Rose Red) ---
    net.add_line(Line(4, "Line 3", [
        s301, s302, s303, s304, s305, s306, s307, s308, s309, s310,
    ], max_speed=80, color=(210, 80, 110)))

    # --- Line 4 / Daxing (Teal) ---
    net.add_line(Line(5, "Line 4", [
        s411, s410, s409, s408, s407, s406, s405, s404, s403, s402,
        s401, s400, s412, s413, s414, s415, s416, s417, s418, s419,
        s420, s421, s422, s423, s424, s425, s426, s427, s428, s429,
        s430, s431, s432, s433, s434,
    ], max_speed=80, color=(0, 150, 150)))

    # --- Line 5 (Purple) ---
    net.add_line(Line(6, "Line 5", [
        s501, s502, s503, s504, s505, s506, s507, s508, s509, s510,
        s511, s512, s513, s514, s515, s516, s517, s518, s519, s520,
        s521, s522, s523,
    ], max_speed=80, color=(160, 50, 160)))

    # --- Line 6 (Earth Yellow) ---
    net.add_line(Line(7, "Line 6", [
        s601, s602, s603, s604, s605, s606, s607, s608, s609, s610,
        s611, s612, s613, s614, s615, s616, s617, s618, s619, s620,
        s621, s622, s623, s624, s625, s626, s627, s628, s629, s630,
        s631, s632, s633, s634,
    ], max_speed=100, color=(180, 150, 40)))

    # --- Line 7 (Light Orange) ---
    net.add_line(Line(8, "Line 7", [
        s701, s702, s703, s704, s705, s706, s707, s708, s709, s710,
        s711, s712, s713, s714, s715, s716, s717, s718, s719, s720,
        s721, s722, s723, s724, s725, s726, s727, s728, s729, s730,
    ], max_speed=80, color=(240, 170, 80)))

    # --- Line 8 (Green) ---
    net.add_line(Line(9, "Line 8", [
        s801, s802, s803, s804, s805, s806, s807, s808, s809, s810,
        s811, s812, s813, s814, s815, s816, s817, s818, s819, s820,
        s821, s822, s823, s824, s825, s826, s827, s828, s829, s830,
        s831, s832, s833, s834, s835,
    ], max_speed=80, color=(40, 150, 90)))

    # --- Line 9 (Light Green) ---
    net.add_line(Line(10, "Line 9", [
        s901, s902, s903, s904, s905, s906, s907, s908, s909, s910,
        s911, s912, s913,
    ], max_speed=80, color=(150, 200, 80)))

    # --- Line 10 (Sky Blue, Loop) ---
    net.add_line(Line(11, "Line 10", [
        s1001, s1002, s1003, s1004, s1005, s1006, s1007, s1008, s1009,
        s1010, s1011, s1012, s1013, s1014, s1015, s1016, s1017, s1018,
        s1019, s1020, s1021, s1022, s1023, s1024, s1025, s1026, s1027,
        s1028, s1029, s1030, s1031, s1032, s1033, s1034, s1035, s1036,
        s1037, s1038, s1039, s1040, s1041, s1042, s1043, s1044, s1045,
        s1001,
    ], max_speed=80, color=(40, 160, 210), ring_label_station_id=1001))

    # --- Line 13 (Yellow) ---
    net.add_line(Line(12, "Line 13", [
        s1301, s1302, s1303, s1304, s1305, s1306, s1307, s1308, s1309,
        s1310, s1311, s1312, s1313, s1314, s1315, s1316, s1317,
    ], max_speed=80, color=(240, 210, 40)))

    # --- Line 14 (Light Pink) ---
    net.add_line(Line(13, "Line 14", [
        s1401, s1402, s1403, s1404, s1405, s1406, s1407, s1408, s1409,
        s1410, s1411, s1412, s1413, s1414, s1415, s1416, s1417, s1418,
        s1419, s1420, s1421, s1422, s1423, s1424, s1425, s1426, s1427,
        s1428, s1429, s1430, s1431, s1432, s1433,
    ], max_speed=80, color=(230, 150, 170)))

    # --- Line 15 (Violet) ---
    net.add_line(Line(14, "Line 15", [
        s1501, s1502, s1503, s1504, s1505, s1506, s1507, s1508, s1509,
        s1510, s1511, s1512, s1513, s1514, s1515, s1516, s1517, s1518,
        s1519, s1520,
    ], max_speed=100, color=(140, 80, 160)))

    # --- Line 16 (Green) ---
    net.add_line(Line(15, "Line 16", [
        s1601, s1602, s1603, s1604, s1605, s1606, s1607, s1608, s1609,
        s1610, s1611, s1612, s1613, s1614, s1615, s1616, s1617, s1618,
        s1619, s1620, s1621, s1622, s1623, s1624, s1625, s1626, s1627,
        s1628, s1629, s1630,
    ], max_speed=80, color=(70, 160, 100)))

    # --- Changping Line (Pink) ---
    net.add_line(Line(16, "Changping Line", [
        s1701, s1702, s1703, s1704, s1705, s1706, s1707, s1708, s1709,
        s1710, s1711, s1712, s1713, s1714, s1715, s1716, s1717, s1718,
        s1719, s1720,
    ], max_speed=100, color=(220, 140, 170)))

    # --- Fangshan Line (Orange) ---
    net.add_line(Line(17, "Fangshan Line", [
        s1801, s1802, s1803, s1804, s1805, s1806, s1807, s1808, s1809,
        s1810, s1811, s1812, s1813, s1814, s1815, s1816,
    ], max_speed=100, color=(230, 140, 60)))

    # --- Yizhuang Line (Peach Pink) ---
    net.add_line(Line(18, "Yizhuang Line", [
        s1901, s1902, s1903, s1904, s1905, s1906, s1907, s1908, s1909,
        s1910, s1911, s1912, s1913, s1914,
    ], max_speed=80, color=(220, 140, 160)))

    # --- Capital Airport Express (Silver) ---
    net.add_line(Line(19, "Airport Express", [
        s2001, s2002, s2003, s2004, s2005,
    ], max_speed=110, color=(170, 170, 180)))

    # --- Daxing Airport Express (Dark Blue) ---
    net.add_line(Line(20, "Daxing Airport Exp.", [
        s2101, s2102, s2103,
    ], max_speed=160, color=(30, 60, 150)))

    # Assign more realistic station types
    _set_station_types(net)

    return net


def _set_station_types(net: MetroNetwork):
    """Heuristic station type assignment based on line and position."""
    # Stations in centre area (Line 2 loop) → underground
    for sid, st in net.stations.items():
        x, y = st.position[0], st.position[1]
        # Far suburban stations → ground or elevated
        if abs(x) > 20 or abs(y) > 20:
            st.station_type = StationType.GROUND
        elif abs(x) > 15 or abs(y) > 15:
            if sid % 3 == 0:
                st.station_type = StationType.ELEVATED
            else:
                st.station_type = StationType.GROUND
        elif abs(x) > 8 or abs(y) > 8:
            if sid % 5 == 0:
                st.station_type = StationType.ELEVATED
        # Inner city → underground (default)
