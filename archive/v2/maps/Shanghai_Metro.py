"""Shanghai Metro — schematic map based on real 2024–2025 network data.

Coverage: Lines 1–18, Pujiang Line, Maglev, Airport Link.

Coordinate system: schematic — centre ~People's Square (0, 0), 1 unit ≈ 1.5 km.
Huangpu River roughly at x ≈ 2 (Puxi west, Pudong east).

Data source: Shanghai Metro official map + Wikipedia station lists (2024–2025).
"""

from main import Station, StationType, Line, MetroNetwork


def _st(net, sid, name, x, y, stype=StationType.UNDERGROUND):
    """Create and register a station. Returns the Station object."""
    if sid in net.stations:
        return net.stations[sid]
    st = Station(sid, name, (x, y), stype)
    net.stations[sid] = st
    return st


def create_shanghai_metro() -> MetroNetwork:
    net = MetroNetwork()

    # =====================================================================
    # Stations — unique IDs.  Transfer stations use the SAME ID across
    # lines for automatic transfer detection.
    # =====================================================================

    # --- Line 1 (Red) — North-South through centre ---
    s101 = _st(net, 101, "Fujin Road",              0.5, 12)
    s102 = _st(net, 102, "Youyi West Road",          0.3, 10.5)
    s103 = _st(net, 103, "Bao'an Highway",           0, 9)
    s104 = _st(net, 104, "Gongfu Xincun",           -0.2, 7.8)
    s105 = _st(net, 105, "Hulan Road",              -0.3, 6.5)
    s106 = _st(net, 106, "Tonghe Xincun",           -0.2, 5.3)
    s107 = _st(net, 107, "Gongkang Road",           -0.1, 4.5)
    s108 = _st(net, 108, "Pengpu Xincun",            0, 3.5)
    s109 = _st(net, 109, "Wenshui Road",             0.2, 2.8)
    s110 = _st(net, 110, "Shanghai Circus World",    0.2, 2)
    s111 = _st(net, 111, "Yanchang Road",            0, 1.3)
    s112 = _st(net, 112, "Zhongshan North Road",    -0.2, 0.5)
    s113 = _st(net, 113, "Shanghai Railway Stn",    -0.5, -0.3)   # X: L3, L4
    s114 = _st(net, 114, "Hanzhong Road",           -0.3, -1)     # X: L12, L13
    s115 = _st(net, 115, "Xinzha Road",             -0.1, -1.5)
    s116 = _st(net, 116, "People's Square",          0, -2.5)     # X: L2, L8
    s117 = _st(net, 117, "S. Huangpi Rd / Site 1",   0.5, -3)     # X: L14
    s118 = _st(net, 118, "South Shaanxi Road",       0, -3.8)     # X: L10, L12
    s119 = _st(net, 119, "Changshu Road",           -0.5, -4.5)   # X: L7
    s120 = _st(net, 120, "Hengshan Road",           -0.3, -5.2)
    s121 = _st(net, 121, "Xujiahui",                 0, -6)       # X: L9, L11
    s122 = _st(net, 122, "Shanghai Indoor Stadium",  0.3, -7)     # X: L4
    s123 = _st(net, 123, "Caobao Road",              0.2, -8)     # X: L12
    s124 = _st(net, 124, "Shanghai South Stn",      -0.5, -9.5)   # X: L3, L15
    s125 = _st(net, 125, "Jinjiang Park",            0.5, -10.5)
    s126 = _st(net, 126, "Lianhua Road",             1, -11.5)
    s127 = _st(net, 127, "Waihuanlu",                1.5, -12)
    s128 = _st(net, 128, "Xinzhuang",                2, -13)      # X: L5

    # --- Line 2 (Light Green) — East-West through centre ---
    s201 = _st(net, 201, "Panxiang Road",           -16.5, 0.5)
    s202 = _st(net, 202, "Nat. Exhib. & Conv. Ctr", -15, 0.5)
    s203 = _st(net, 203, "Hongqiao Railway Stn",    -14, 1)       # X: L10, L17
    s204 = _st(net, 204, "Hongqiao Airport T2",     -12.5, 1)     # X: L10
    s205 = _st(net, 205, "Songhong Road",           -10, 0)
    s206 = _st(net, 206, "Beixinjing",               -8.5, -0.3)
    s207 = _st(net, 207, "Weining Road",             -7, -0.5)
    s208 = _st(net, 208, "Loushanguan Road",         -5.5, -1)    # X: L15
    s209 = _st(net, 209, "Zhongshan Park",           -4, -1.5)    # X: L3, L4
    s210 = _st(net, 210, "Jiangsu Road",             -3, -2)      # X: L11
    s211 = _st(net, 211, "Jing'an Temple",           -1.5, -2.5)  # X: L7, L14
    s212 = _st(net, 212, "West Nanjing Road",        -0.8, -2.5)  # X: L12, L13
    s213 = _st(net, 213, "People's Square",           0, -2.5)    # X: L1, L8
    s214 = _st(net, 214, "East Nanjing Road",         1.5, -2)    # X: L10
    s215 = _st(net, 215, "Lujiazui",                  3, -2)      # X: L14
    s216 = _st(net, 216, "Pudong South Road",         4, -2.3)    # X: L14
    s217 = _st(net, 217, "Century Avenue",            5, -2.5)    # X: L4, L6, L9
    s218 = _st(net, 218, "Shanghai Sci. & Tech",      6.5, -2.5)
    s219 = _st(net, 219, "Century Park",              7.5, -2.5)
    s220 = _st(net, 220, "Longyang Road",             8.5, -3)    # X: L7, L16, L18, Maglev
    s221 = _st(net, 221, "Zhangjiang Hi-Tech",        10, -3)
    s222 = _st(net, 222, "Jinke Road",                11, -3)
    s223 = _st(net, 223, "Guanglan Road",             12.5, -2.5)
    s224 = _st(net, 224, "Tangzhen",                  14, -2)
    s225 = _st(net, 225, "Chuangxin Middle Road",     15.5, -1.5)
    s226 = _st(net, 226, "Huaxia East Road",          17, -1)
    s227 = _st(net, 227, "Chuansha",                  18.5, -0.5)
    s228 = _st(net, 228, "Lingkong Road",             20, 0)
    s229 = _st(net, 229, "Yuandong Avenue",           21, 0.3)
    s230 = _st(net, 230, "Haitian 3rd Road",          22, 0.5)
    s231 = _st(net, 231, "Pudong Airport T1&T2",     23.5, 0.8)  # X: Maglev, Airport Link

    # --- Line 3 (Yellow) — North-South west side ---
    s301 = _st(net, 301, "North Jiangyang Road",     -1.5, 12.5)
    s302 = _st(net, 302, "Tieli Road",               -1.3, 11.5)
    s303 = _st(net, 303, "Youyi Road",               -1.2, 10.5)
    s304 = _st(net, 304, "Baoyang Road",             -1.1, 9.5)
    s305 = _st(net, 305, "Shuichan Road",            -1, 8.5)
    s306 = _st(net, 306, "Songbin Road",             -0.8, 7.3)
    s307 = _st(net, 307, "Zhanghuabang",             -0.6, 6.2)
    s308 = _st(net, 308, "Songfa Road",              -0.6, 5.3)
    s309 = _st(net, 309, "South Changjiang Road",    -0.5, 4.5)   # X: L18
    s310 = _st(net, 310, "West Yingao Road",         -0.5, 3.5)
    s311 = _st(net, 311, "Jiangwan Town",            -0.8, 2.5)
    s312 = _st(net, 312, "Dabaishu",                 -1, 1.5)
    s313 = _st(net, 313, "Chifeng Road",             -1, 0.8)
    s314 = _st(net, 314, "Hongkou Football Stdm",    -0.8, 0)      # X: L8
    s315 = _st(net, 315, "Dongbaoxing Road",         -0.8, -0.5)
    s316 = _st(net, 316, "Baoshan Road",             -0.8, -0.8)   # X: L4
    s317 = _st(net, 317, "Shanghai Railway Stn",     -0.5, -0.3)   # X: L1, L4
    s318 = _st(net, 318, "Zhongtan Road",            -1.5, -1)     # X: L4
    s319 = _st(net, 319, "Zhenping Road",            -2.5, -1.5)   # X: L4, L7
    s320 = _st(net, 320, "Caoyang Road",             -3.2, -1.5)   # X: L4, L11, L14
    s321 = _st(net, 321, "Jinshajiang Road",         -3.8, -1.5)   # X: L4, L13
    s322 = _st(net, 322, "Zhongshan Park",           -4, -1.5)     # X: L2, L4
    s323 = _st(net, 323, "West Yan'an Road",         -4.5, -2)     # X: L4
    s324 = _st(net, 324, "Hongqiao Road",            -4.5, -3.3)   # X: L4, L10
    s325 = _st(net, 325, "Yishan Road",              -4, -4.2)     # X: L4, L9
    s326 = _st(net, 326, "Caoxi Road",               -3.5, -5.5)
    s327 = _st(net, 327, "Longcao Road",             -2.5, -6.5)   # X: L12
    s328 = _st(net, 328, "Shilong Road",             -1.5, -7.5)
    s329 = _st(net, 329, "Shanghai South Stn",       -0.5, -9.5)   # X: L1, L15

    # --- Line 4 (Dark Purple) — Loop ---
    # Using same station IDs as L3 for shared stations (Baoshan Rd → Yishan Rd)
    s401 = _st(net, 401, "Shanghai Indoor Stadium",   0.3, -7)     # X: L1
    s402 = _st(net, 402, "Shanghai Stadium",          0.8, -6)
    s403 = _st(net, 403, "Dong'an Road",              1.5, -5)     # X: L7
    s404 = _st(net, 404, "Damuqiao Road",             1.8, -4)     # X: L12
    s405 = _st(net, 405, "Luban Road",                2.2, -3)
    s406 = _st(net, 406, "South Xizang Road",         2.5, -2.5)   # X: L8
    s407 = _st(net, 407, "Nanpu Bridge",              3, -2)
    s408 = _st(net, 408, "Tangqiao",                  3.5, -2)
    s409 = _st(net, 409, "Lancun Road",               4.2, -2.2)   # X: L6
    s410 = _st(net, 410, "Xiangcheng Road",           4.5, -2.3)
    s411 = _st(net, 411, "Century Avenue",            5, -2.5)     # X: L2, L6, L9
    s412 = _st(net, 412, "Pudong Avenue",             4.5, -1.5)   # X: L14
    s413 = _st(net, 413, "Yangshupu Road",            4, 0)
    s414 = _st(net, 414, "Dalian Road",               3.5, 0.5)    # X: L12
    s415 = _st(net, 415, "Linping Road",              3, 1.2)
    s416 = _st(net, 416, "Hailun Road",               2.5, 1.5)    # X: L10
    # L3 shared section: Baoshan Rd → Yishan Rd
    # s316 (Baoshan Rd), s317 (Shanghai Railway Stn), s318 (Zhongtan),
    # s319 (Zhenping Rd), s320 (Caoyang Rd), s321 (Jinshajiang Rd),
    # s322 (Zhongshan Park), s323 (West Yan'an Rd), s324 (Hongqiao Rd),
    # s325 (Yishan Rd)
    # then back to s401

    # --- Line 5 (Purple) — Southern branch ---
    s501 = _st(net, 501, "Xinzhuang",                2, -13)       # X: L1
    s502 = _st(net, 502, "Chunshen Road",            2.5, -14)
    s503 = _st(net, 503, "Yindu Road",               3, -15)
    s504 = _st(net, 504, "Zhuanqiao",                3.5, -16)
    s505 = _st(net, 505, "Beiqiao",                  4, -17)
    s506 = _st(net, 506, "Jianchuan Road",           4.5, -18)
    s507 = _st(net, 507, "Dongchuan Road",           5, -19)       # X: branch
    s508 = _st(net, 508, "Jiangchuan Road",          5.5, -20)
    s509 = _st(net, 509, "Xidu",                     5.5, -21.5)
    s510 = _st(net, 510, "Xiaotang",                 5.5, -23)
    s511 = _st(net, 511, "Fengpu Avenue",            5.5, -24.5)
    s512 = _st(net, 512, "Huancheng East Road",      5.5, -26)
    s513 = _st(net, 513, "Wangyuan Road",            5.5, -27.5)
    s514 = _st(net, 514, "Jinhai Lake",              6, -29)
    s515 = _st(net, 515, "Fengxian Xincheng",        6.5, -30.5)
    # Branch
    s516 = _st(net, 516, "Jinping Road",             4, -19.5)
    s517 = _st(net, 517, "Huaning Road",             3.5, -20)
    s518 = _st(net, 518, "Wenjing Road",             3, -20.3)
    s519 = _st(net, 519, "Minhang Dev. Zone",        2.5, -20.5)

    # --- Line 6 (Pinkish Red) — North-South Pudong ---
    s601 = _st(net, 601, "Gangcheng Road",          8, 11.5)       # X: L10
    s602 = _st(net, 602, "N. Waigaoqiao FTZ",       8, 10)
    s603 = _st(net, 603, "Hangjin Road",            7.5, 8.5)
    s604 = _st(net, 604, "S. Waigaoqiao FTZ",       7.5, 7)
    s605 = _st(net, 605, "Zhouhai Road",            7, 5.5)
    s606 = _st(net, 606, "Wuzhou Avenue",           7, 4.5)
    s607 = _st(net, 607, "Dongjing Road",           6.5, 3.5)
    s608 = _st(net, 608, "Jufeng Road",             6, 2.5)       # X: L12
    s609 = _st(net, 609, "Wulian Road",             5.8, 1.5)
    s610 = _st(net, 610, "Boxing Road",             5.5, 0.5)
    s611 = _st(net, 611, "Jinqiao Road",            5.3, -0.5)
    s612 = _st(net, 612, "Yunshan Road",            5, -1.3)      # X: L14
    s613 = _st(net, 613, "Deping Road",             4.8, -1.8)
    s614 = _st(net, 614, "Beiyangjing Road",        4.6, -2)
    s615 = _st(net, 615, "Minsheng Road",           4.4, -2.2)    # X: L18
    s616 = _st(net, 616, "Yuanshen Sports Ctr",     4.5, -2.3)
    s617 = _st(net, 617, "Century Avenue",          5, -2.5)      # X: L2, L4, L9
    s618 = _st(net, 618, "Pudian Road",             5.2, -2.8)
    s619 = _st(net, 619, "Lancun Road",             4.2, -2.2)    # X: L4
    s620 = _st(net, 620, "Children's Medical Ctr",  4.5, -3.5)
    s621 = _st(net, 621, "Linyi Xincun",            5, -4.2)
    s622 = _st(net, 622, "West Gaoke Road",         5.5, -5)      # X: L7
    s623 = _st(net, 623, "Dongming Road",           6, -5.5)      # X: L13
    s624 = _st(net, 624, "Gaoqing Road",            6.5, -6)
    s625 = _st(net, 625, "West Huaxia Road",        7, -6.5)
    s626 = _st(net, 626, "Shangnan Road",           7.5, -7)
    s627 = _st(net, 627, "South Lingyan Road",      8, -7.5)
    s628 = _st(net, 628, "Oriental Sports Center",  8.5, -8)      # X: L8, L11

    # --- Line 7 (Orange) — Northwest-Southeast ---
    s701 = _st(net, 701, "Meilan Lake",             -9, 12)
    s702 = _st(net, 702, "Luonan Xincun",           -8, 10.5)
    s703 = _st(net, 703, "Panguang Road",           -7, 9)
    s704 = _st(net, 704, "Liuhang",                 -6, 7.5)
    s705 = _st(net, 705, "Gucun Park",              -5, 6.5)      # X: L15
    s706 = _st(net, 706, "Qihua Road",              -4.5, 5.5)
    s707 = _st(net, 707, "Shanghai University",     -4, 4.5)
    s708 = _st(net, 708, "Nanchen Road",            -3.5, 3.5)
    s709 = _st(net, 709, "Shangda Road",            -3, 2.5)
    s710 = _st(net, 710, "Changzhong Road",         -2.8, 1.5)
    s711 = _st(net, 711, "Dachang Town",            -2.5, 0.8)
    s712 = _st(net, 712, "Xingzhi Road",            -2.5, 0)
    s713 = _st(net, 713, "Dahuasan Road",           -2.5, -0.5)
    s714 = _st(net, 714, "Xincun Road",             -2.8, -1)
    s715 = _st(net, 715, "Langao Road",             -2.8, -1.5)
    s716 = _st(net, 716, "Zhenping Road",           -2.5, -1.5)   # X: L3, L4
    s717 = _st(net, 717, "Changshou Road",          -2, -2)       # X: L13
    s718 = _st(net, 718, "Changping Road",          -2, -2.3)
    s719 = _st(net, 719, "Jing'an Temple",          -1.5, -2.5)   # X: L2, L14
    s720 = _st(net, 720, "Changshu Road",           -0.5, -4.5)   # X: L1
    s721 = _st(net, 721, "Zhaojiabang Road",         0, -5)       # X: L9
    s722 = _st(net, 722, "Dong'an Road",            1.5, -5)      # X: L4
    s723 = _st(net, 723, "Middle Longhua Road",     2.5, -5)      # X: L12
    s724 = _st(net, 724, "Houtan",                  4, -6)
    s725 = _st(net, 725, "Changqing Road",          5, -5.5)      # X: L13
    s726 = _st(net, 726, "Yaohua Road",             5.5, -5)      # X: L8
    s727 = _st(net, 727, "Yuntai Road",             6, -4.5)
    s728 = _st(net, 728, "West Gaoke Road",         5.5, -5)      # X: L6
    s729 = _st(net, 729, "South Yanggao Road",      6.5, -4)
    s730 = _st(net, 730, "Jinxiu Road",             7, -3.5)
    s731 = _st(net, 731, "Fanghua Road",            7.5, -3)
    s732 = _st(net, 732, "Longyang Road",           8.5, -3)      # X: L2, L16, L18, Maglev
    s733 = _st(net, 733, "Huamu Road",              9, -2.5)

    # --- Line 8 (Steel Blue) — North-South through centre ---
    s801 = _st(net, 801, "Shiguang Road",           3, 9)
    s802 = _st(net, 802, "Nenjiang Road",           2.8, 7.8)
    s803 = _st(net, 803, "Xiangyin Road",           2.5, 6.5)
    s804 = _st(net, 804, "Huangxing Park",          2.2, 5.3)
    s805 = _st(net, 805, "Middle Yanji Road",       2, 4)
    s806 = _st(net, 806, "Huangxing Road",          1.5, 3)
    s807 = _st(net, 807, "Jiangpu Road",            1, 2)        # X: L18
    s808 = _st(net, 808, "Anshan Xincun",           0.5, 1.2)
    s809 = _st(net, 809, "Siping Road",             0, 0.5)      # X: L10
    s810 = _st(net, 810, "Quyang Road",            -0.5, 0)
    s811 = _st(net, 811, "Hongkou Football Stdm",  -0.8, 0)      # X: L3
    s812 = _st(net, 812, "North Xizang Road",      -0.5, -0.8)
    s813 = _st(net, 813, "Zhongxing Road",         -0.2, -1.3)
    s814 = _st(net, 814, "Qufu Road",               0, -1.8)     # X: L12
    s815 = _st(net, 815, "People's Square",         0, -2.5)     # X: L1, L2
    s816 = _st(net, 816, "Dashijie",                0.5, -3.2)   # X: L14
    s817 = _st(net, 817, "Laoximen",                1, -3.5)     # X: L10
    s818 = _st(net, 818, "Lujiabang Road",          1.5, -4)     # X: L9
    s819 = _st(net, 819, "South Xizang Road",       2.5, -2.5)   # X: L4
    s820 = _st(net, 820, "China Art Museum",        3.8, -3.5)
    s821 = _st(net, 821, "Yaohua Road",             5.5, -5)     # X: L7
    s822 = _st(net, 822, "Chengshan Road",          6, -5.5)     # X: L13
    s823 = _st(net, 823, "Yangsi",                  6.5, -6.5)
    s824 = _st(net, 824, "Oriental Sports Center",  8.5, -8)     # X: L6, L11
    s825 = _st(net, 825, "Lingzhao Xincun",         9, -9.5)
    s826 = _st(net, 826, "Luheng Road",             9.5, -11)
    s827 = _st(net, 827, "Pujiang Town",            10, -12.5)
    s828 = _st(net, 828, "Jiangyue Road",           10.5, -14)
    s829 = _st(net, 829, "Lianhang Road",           11, -15.5)
    s830 = _st(net, 830, "Shendu Highway",          11.5, -17)   # X: Pujiang

    # --- Line 9 (Light Sky Blue) — West-East through centre ---
    s901 = _st(net, 901, "Shanghai Songjiang Stn", -20, -8)
    s902 = _st(net, 902, "Zuibaichi",              -18.5, -8)
    s903 = _st(net, 903, "Songjiang Sports Ctr",   -17, -7.5)
    s904 = _st(net, 904, "Songjiang Xincheng",     -15.5, -7)
    s905 = _st(net, 905, "Songjiang Univ. Town",   -14, -6.5)
    s906 = _st(net, 906, "Dongjing",               -12, -6)
    s907 = _st(net, 907, "Sheshan",                -10.5, -5.5)
    s908 = _st(net, 908, "Sijing",                 -9, -5.3)
    s909 = _st(net, 909, "Jiuting",                -7, -5.2)
    s910 = _st(net, 910, "Zhongchun Road",         -5.5, -5)     # X: Airport Link
    s911 = _st(net, 911, "Qibao",                  -4.5, -4.8)
    s912 = _st(net, 912, "Xingzhong Road",         -3.8, -4.5)
    s913 = _st(net, 913, "Hechuan Road",           -3.2, -4.3)
    s914 = _st(net, 914, "Caohejing Hi-Tech",      -3.5, -4.5)
    s915 = _st(net, 915, "Guilin Road",            -3.5, -4.5)   # X: L15
    s916 = _st(net, 916, "Yishan Road",            -4, -4.2)     # X: L3, L4
    s917 = _st(net, 917, "Xujiahui",                0, -6)       # X: L1, L11
    s918 = _st(net, 918, "Zhaojiabang Road",        0, -5)       # X: L7
    s919 = _st(net, 919, "Jiashan Road",            1, -4.5)     # X: L12
    s920 = _st(net, 920, "Dapuqiao",                1.5, -4)
    s921 = _st(net, 921, "Madang Road",             1.5, -3.5)   # X: L13
    s922 = _st(net, 922, "Lujiabang Road",          1.5, -4)     # X: L8
    s923 = _st(net, 923, "Xiaonanmen",              2, -2.8)
    s924 = _st(net, 924, "Shangcheng Road",         3.5, -2.5)
    s925 = _st(net, 925, "Century Avenue",          5, -2.5)     # X: L2, L4, L6
    s926 = _st(net, 926, "Middle Yanggao Road",     6, -2.5)     # X: L18
    s927 = _st(net, 927, "Fangdian Road",           7.5, -2.5)
    s928 = _st(net, 928, "Lantian Road",            9, -2)       # X: L14
    s929 = _st(net, 929, "Taierzhuang Road",        10.5, -1.5)
    s930 = _st(net, 930, "Jinqiao",                 12, -1)
    s931 = _st(net, 931, "Jinji Road",              13, -0.5)
    s932 = _st(net, 932, "Jinhai Road",             14, 0)       # X: L12
    s933 = _st(net, 933, "Gutang Road",             15.5, 0.5)
    s934 = _st(net, 934, "Minlei Road",             17, 0.8)
    s935 = _st(net, 935, "Caolu",                   18.5, 1)

    # --- Line 10 (Light Purple) — West-East through north centre ---
    s1001 = _st(net, 1001, "Hongqiao Railway Stn",  -14, 1)      # X: L2, L17
    s1002 = _st(net, 1002, "Hongqiao Airport T2",   -12.5, 1)    # X: L2
    s1003 = _st(net, 1003, "Hongqiao Airport T1",   -11, 1.5)
    s1004 = _st(net, 1004, "Shanghai Zoo",          -8, 1.5)
    s1005 = _st(net, 1005, "Longxi Road",           -7, 1)       # X: branch
    s1006 = _st(net, 1006, "Shuicheng Road",        -6, 0.5)
    s1007 = _st(net, 1007, "Yili Road",             -5.5, 0)
    s1008 = _st(net, 1008, "Songyuan Road",         -5, -0.8)
    s1009 = _st(net, 1009, "Hongqiao Road",         -4.5, -3.3)  # X: L3, L4
    s1010 = _st(net, 1010, "Jiaotong University",   -3, -3.5)    # X: L11
    s1011 = _st(net, 1011, "Shanghai Library",      -1.5, -3.5)
    s1012 = _st(net, 1012, "South Shaanxi Road",     0, -3.8)    # X: L1, L12
    s1013 = _st(net, 1013, "Site 1 · Xintiandi",     1, -3.2)    # X: L13
    s1014 = _st(net, 1014, "Laoximen",               1, -3.5)    # X: L8
    s1015 = _st(net, 1015, "Yuyuan Garden",          1.8, -2.5)  # X: L14
    s1016 = _st(net, 1016, "East Nanjing Road",      1.5, -2)    # X: L2
    s1017 = _st(net, 1017, "Tiantong Road",          1.8, -1)    # X: L12
    s1018 = _st(net, 1018, "North Sichuan Road",     2, 0.2)
    s1019 = _st(net, 1019, "Hailun Road",            2.5, 1.5)   # X: L4
    s1020 = _st(net, 1020, "Youdian Xincun",         2.8, 2.5)
    s1021 = _st(net, 1021, "Siping Road",            0, 0.5)     # X: L8
    s1022 = _st(net, 1022, "Tongji University",      0.5, 1.5)
    s1023 = _st(net, 1023, "Guoquan Road",           1, 2.2)     # X: L18
    s1024 = _st(net, 1024, "Wujiaochang",            1.5, 3)
    s1025 = _st(net, 1025, "Jiangwan Stadium",       2, 4)
    s1026 = _st(net, 1026, "Sanmen Road",            2.5, 5)
    s1027 = _st(net, 1027, "East Yingao Road",       3, 6)
    s1028 = _st(net, 1028, "Xinjiangwancheng",       4, 7)
    s1029 = _st(net, 1029, "Guofan Road",            5, 8)
    s1030 = _st(net, 1030, "Shuangjiang Road",       6, 9)
    s1031 = _st(net, 1031, "West Gaoqiao",           7, 10)
    s1032 = _st(net, 1032, "Gaoqiao",                7.5, 10.5)
    s1033 = _st(net, 1033, "Gangcheng Road",         8, 11.5)    # X: L6
    s1034 = _st(net, 1034, "Jilong Road",            8.5, 12.5)
    # Branch
    s1035 = _st(net, 1035, "Longbai Xincun",        -7.5, 0.3)
    s1036 = _st(net, 1036, "Ziteng Road",           -8, 0)
    s1037 = _st(net, 1037, "Hangzhong Road",        -8.5, -0.3)

    # --- Line 11 (Dark Red) — Northwest-Southeast + Kunshan branch ---
    s1101 = _st(net, 1101, "North Jiading",        -13, 9.5)
    s1102 = _st(net, 1102, "West Jiading",         -12.5, 8.5)
    s1103 = _st(net, 1103, "Baiyin Road",          -12, 7)
    s1104 = _st(net, 1104, "Jiading Xincheng",     -11.5, 6)     # X: branch
    s1105 = _st(net, 1105, "Malu",                 -10.5, 4.5)
    s1106 = _st(net, 1106, "Chenxiang Highway",    -9.5, 3.5)
    s1107 = _st(net, 1107, "Nanxiang",             -8.5, 2.5)
    s1108 = _st(net, 1108, "Taopu Xincun",         -7, 1.5)
    s1109 = _st(net, 1109, "Wuwei Road",           -6.5, 0.5)
    s1110 = _st(net, 1110, "Qilianshan Road",      -6, -0.2)
    s1111 = _st(net, 1111, "Liziyuan",             -5.5, -0.8)
    s1112 = _st(net, 1112, "Shanghai West Stn",    -5, -1.5)     # X: L15
    s1113 = _st(net, 1113, "Zhenru",               -4.5, -1.5)   # X: L14
    s1114 = _st(net, 1114, "Fengqiao Road",        -4, -1.8)
    s1115 = _st(net, 1115, "Caoyang Road",         -3.2, -1.5)   # X: L3, L4, L14
    s1116 = _st(net, 1116, "Longde Road",          -3.5, -2)     # X: L13
    s1117 = _st(net, 1117, "Jiangsu Road",         -3, -2)       # X: L2
    s1118 = _st(net, 1118, "Jiaotong University",  -3, -3.5)     # X: L10
    s1119 = _st(net, 1119, "Xujiahui",              0, -6)       # X: L1, L9
    s1120 = _st(net, 1120, "Shanghai Swimming Ctr", 1, -6.5)
    s1121 = _st(net, 1121, "Longhua",               1.5, -5.5)   # X: L12
    s1122 = _st(net, 1122, "Yunjin Road",           2, -6)
    s1123 = _st(net, 1123, "Longyao Road",          3, -7)
    s1124 = _st(net, 1124, "Oriental Sports Ctr",   8.5, -8)     # X: L6, L8
    s1125 = _st(net, 1125, "Sanlin",                8, -9)
    s1126 = _st(net, 1126, "East Sanlin",           8.5, -10)
    s1127 = _st(net, 1127, "Pusan Road",            9, -11)
    s1128 = _st(net, 1128, "Kangheng Road",         9.5, -12)
    s1129 = _st(net, 1129, "Yuqiao",                10, -13)     # X: L18
    s1130 = _st(net, 1130, "Luoshan Road",          10.5, -14)   # X: L16
    s1131 = _st(net, 1131, "Xiuyan Road",           11, -15)
    s1132 = _st(net, 1132, "Kangxin Highway",       11.5, -16)
    s1133 = _st(net, 1133, "Disney Resort",         12.5, -17)
    # Kunshan branch
    s1134 = _st(net, 1134, "Shanghai Circuit",     -12.5, 5)
    s1135 = _st(net, 1135, "East Changji Road",    -12, 3.5)
    s1136 = _st(net, 1136, "Shanghai Auto City",   -12, 2)
    s1137 = _st(net, 1137, "Anting",               -11.5, 0.5)
    s1138 = _st(net, 1138, "Zhaofeng Road",        -11.5, -0.8)
    s1139 = _st(net, 1139, "Guangming Road",       -11.5, -1.5)
    s1140 = _st(net, 1140, "Huaqiao",              -11.5, -2.5)

    # --- Line 12 (Dark Green) — West-East ---
    s1201 = _st(net, 1201, "Qixin Road",           -9, -5)
    s1202 = _st(net, 1202, "Hongxin Road",         -7.5, -4.8)
    s1203 = _st(net, 1203, "Gudai Road",           -6, -4.5)
    s1204 = _st(net, 1204, "Donglan Road",         -5, -4.8)
    s1205 = _st(net, 1205, "Hongmei Road",         -4.5, -5)
    s1206 = _st(net, 1206, "Hongcao Road",         -4, -5.2)
    s1207 = _st(net, 1207, "Guilin Park",          -3.5, -5)     # X: L15
    s1208 = _st(net, 1208, "Caobao Road",           0.2, -8)     # X: L1
    s1209 = _st(net, 1209, "Longcao Road",         -2.5, -6.5)   # X: L3
    s1210 = _st(net, 1210, "Longhua",               1.5, -5.5)   # X: L11
    s1211 = _st(net, 1211, "Middle Longhua Road",   2.5, -5)     # X: L7
    s1212 = _st(net, 1212, "Damuqiao Road",         1.8, -4)     # X: L4
    s1213 = _st(net, 1213, "Jiashan Road",          1, -4.5)     # X: L9
    s1214 = _st(net, 1214, "South Shaanxi Road",    0, -3.8)     # X: L1, L10
    s1215 = _st(net, 1215, "West Nanjing Road",    -0.8, -2.5)   # X: L2, L13
    s1216 = _st(net, 1216, "Hanzhong Road",        -0.3, -1)     # X: L1, L13
    s1217 = _st(net, 1217, "Qufu Road",             0, -1.8)     # X: L8
    s1218 = _st(net, 1218, "Tiantong Road",         1.8, -1)     # X: L10
    s1219 = _st(net, 1219, "Intl. Cruise Terminal", 2.5, -0.5)
    s1220 = _st(net, 1220, "Tilanqiao",             3, 0)
    s1221 = _st(net, 1221, "Dalian Road",           3.5, 0.5)    # X: L4
    s1222 = _st(net, 1222, "Jiangpu Park",          4, 1)        # X: L18
    s1223 = _st(net, 1223, "Ningguo Road",          4.5, 1.5)
    s1224 = _st(net, 1224, "Longchang Road",        5, 2)
    s1225 = _st(net, 1225, "Aiguo Road",            5.5, 2.5)
    s1226 = _st(net, 1226, "Fuxing Island",         6, 2.5)
    s1227 = _st(net, 1227, "Donglu Road",           6.2, 2.3)
    s1228 = _st(net, 1228, "Jufeng Road",           6, 2.5)      # X: L6
    s1229 = _st(net, 1229, "North Yanggao Road",    7, 2)
    s1230 = _st(net, 1230, "Jinjing Road",          8, 1.5)
    s1231 = _st(net, 1231, "Shenjiang Road",        9, 1)
    s1232 = _st(net, 1232, "Jinhai Road",          14, 0)        # X: L9

    # --- Line 13 (Pink) — West-East ---
    s1301 = _st(net, 1301, "Jinyun Road",         -9.5, -2)
    s1302 = _st(net, 1302, "West Jinshajiang Rd", -8.5, -2)
    s1303 = _st(net, 1303, "Fengzhuang",          -7.5, -2)
    s1304 = _st(net, 1304, "South Qilianshan Rd", -6.5, -1.8)
    s1305 = _st(net, 1305, "Zhenbei Road",        -5.5, -1.8)
    s1306 = _st(net, 1306, "Daduhe Road",         -4.5, -1.8)   # X: L15
    s1307 = _st(net, 1307, "Jinshajiang Road",    -3.8, -1.5)   # X: L3, L4
    s1308 = _st(net, 1308, "Longde Road",         -3.5, -2)     # X: L11
    s1309 = _st(net, 1309, "Wuning Road",         -3, -2.2)     # X: L14
    s1310 = _st(net, 1310, "Changshou Road",      -2, -2)       # X: L7
    s1311 = _st(net, 1311, "Jiangning Road",      -1.5, -2.2)
    s1312 = _st(net, 1312, "Hanzhong Road",       -0.3, -1)     # X: L1, L12
    s1313 = _st(net, 1313, "Natural History Mus.", -0.5, -2)
    s1314 = _st(net, 1314, "West Nanjing Road",   -0.8, -2.5)   # X: L2, L12
    s1315 = _st(net, 1315, "Middle Huaihai Road", -0.2, -3.2)
    s1316 = _st(net, 1316, "Site 1 · Xintiandi",   1, -3.2)     # X: L10
    s1317 = _st(net, 1317, "Madang Road",          1.5, -3.5)   # X: L9
    s1318 = _st(net, 1318, "Expo Museum",          2.8, -3.5)
    s1319 = _st(net, 1319, "Shibo Avenue",         3.5, -4)
    s1320 = _st(net, 1320, "Changqing Road",       5, -5.5)     # X: L7
    s1321 = _st(net, 1321, "Chengshan Road",       6, -5.5)     # X: L8
    s1322 = _st(net, 1322, "Dongming Road",        6, -5.5)     # X: L6
    s1323 = _st(net, 1323, "Huapeng Road",         6.5, -6.5)
    s1324 = _st(net, 1324, "Xianan Road",          7, -7.5)
    s1325 = _st(net, 1325, "Beicai",               7.5, -8.5)
    s1326 = _st(net, 1326, "Chenchun Road",        8, -9.5)
    s1327 = _st(net, 1327, "Lianxi Road",          8.5, -10.5)  # X: L18
    s1328 = _st(net, 1328, "Middle Huaxia Road",   9, -11.5)    # X: L16
    s1329 = _st(net, 1329, "Zhongke Road",         9.5, -12.5)
    s1330 = _st(net, 1330, "Xuelin Road",          10, -13.5)
    s1331 = _st(net, 1331, "Zhangjiang Road",      10.5, -14.5)

    # --- Line 14 (Olive Green) — West-East through centre ---
    s1401 = _st(net, 1401, "Fengbang",            -13, -2.5)
    s1402 = _st(net, 1402, "Lexiu Road",          -11.5, -2.5)
    s1403 = _st(net, 1403, "Lintao Road",         -10, -2.5)
    s1404 = _st(net, 1404, "Jiayi Road",          -8.5, -2.5)
    s1405 = _st(net, 1405, "Dingbian Road",       -7.5, -2.5)
    s1406 = _st(net, 1406, "Zhenxin Xincun",      -6.5, -2.3)
    s1407 = _st(net, 1407, "Zhenguang Road",      -5.8, -2)
    s1408 = _st(net, 1408, "Tongchuan Road",      -5, -1.8)     # X: L15
    s1409 = _st(net, 1409, "Zhenru",              -4.5, -1.5)   # X: L11
    s1410 = _st(net, 1410, "Zhongning Road",      -4, -1.8)
    s1411 = _st(net, 1411, "Caoyang Road",        -3.2, -1.5)   # X: L3, L4, L11
    s1412 = _st(net, 1412, "Wuning Road",         -3, -2.2)     # X: L13
    s1413 = _st(net, 1413, "Wuding Road",         -2, -2.3)
    s1414 = _st(net, 1414, "Jing'an Temple",      -1.5, -2.5)   # X: L2, L7
    s1415 = _st(net, 1415, "S. Huangpi Rd / Site", 0.5, -3)     # X: L1
    s1416 = _st(net, 1416, "Dashijie",             0.5, -3.2)   # X: L8
    s1417 = _st(net, 1417, "Yuyuan Garden",        1.8, -2.5)   # X: L10
    s1418 = _st(net, 1418, "Lujiazui",             3, -2)       # X: L2
    s1419 = _st(net, 1419, "Pudong South Road",    4, -2.3)     # X: L2
    s1420 = _st(net, 1420, "Pudong Avenue",        4.5, -1.5)   # X: L4
    s1421 = _st(net, 1421, "Yuanshen Road",        4.8, -1)
    s1422 = _st(net, 1422, "Changyi Road",         5, 0)        # X: L18
    s1423 = _st(net, 1423, "Xiepu Road",           5.5, 0.5)
    s1424 = _st(net, 1424, "Yunshan Road",         5, -1.3)     # X: L6
    s1425 = _st(net, 1425, "Lantian Road",         9, -2)       # X: L9
    s1426 = _st(net, 1426, "Huangyang Road",       7, -0.5)
    s1427 = _st(net, 1427, "Yunshun Road",         8, 0)
    s1428 = _st(net, 1428, "Pudong Football Stdm", 9, 0.5)
    s1429 = _st(net, 1429, "Jinyue Road",          10, 1)
    s1430 = _st(net, 1430, "Guiqiao Road",         11, 1.5)

    # --- Line 15 (Dark Orange) — North-South west side ---
    s1501 = _st(net, 1501, "Gucun Park",          -5, 6.5)      # X: L7
    s1502 = _st(net, 1502, "Jinqiu Road",         -4.5, 5.3)
    s1503 = _st(net, 1503, "Fengxiang Road",      -4.5, 4)
    s1504 = _st(net, 1504, "Nanda Road",          -4.5, 2.8)
    s1505 = _st(net, 1505, "Qi'an Road",          -4.3, 1.5)
    s1506 = _st(net, 1506, "Gulang Road",         -4.3, 0.3)
    s1507 = _st(net, 1507, "East Wuwei Road",     -4.5, -0.5)
    s1508 = _st(net, 1508, "Shanghai West Stn",   -5, -1.5)     # X: L11
    s1509 = _st(net, 1509, "Tongchuan Road",      -5, -1.8)     # X: L14
    s1510 = _st(net, 1510, "North Meiling Road",  -4.8, -1.5)
    s1511 = _st(net, 1511, "Daduhe Road",         -4.5, -1.8)   # X: L13
    s1512 = _st(net, 1512, "Changfeng Park",      -4.2, -1.5)
    s1513 = _st(net, 1513, "Loushanguan Road",    -5.5, -1)     # X: L2
    s1514 = _st(net, 1514, "Hongbaoshi Road",     -5, -2.5)
    s1515 = _st(net, 1515, "Yaohong Road",        -4.5, -3.5)
    s1516 = _st(net, 1516, "Wuzhong Road",        -4, -4)
    s1517 = _st(net, 1517, "Guilin Road",         -3.5, -4.5)   # X: L9
    s1518 = _st(net, 1518, "Guilin Park",         -3.5, -5)     # X: L12
    s1519 = _st(net, 1519, "Shanghai South Stn",  -0.5, -9.5)   # X: L1, L3
    s1520 = _st(net, 1520, "ECUST",               -0.8, -10.5)
    s1521 = _st(net, 1521, "Luoxiu Road",         -0.5, -11.5)
    s1522 = _st(net, 1522, "Zhumei Road",          0, -12.5)
    s1523 = _st(net, 1523, "Jinghong Road",        1, -13.5)    # X: Airport Link
    s1524 = _st(net, 1524, "South Hongmei Road",   2, -14.5)
    s1525 = _st(net, 1525, "Jingxi Road",          2.5, -15.5)
    s1526 = _st(net, 1526, "Shujian Road",         3, -16.5)
    s1527 = _st(net, 1527, "Shuangbai Road",       3.5, -17.5)
    s1528 = _st(net, 1528, "Yuanjiang Road",       4, -18.5)
    s1529 = _st(net, 1529, "Yongde Road",          4.5, -19.5)
    s1530 = _st(net, 1530, "Zizhu Hi-tech Park",   5, -20.5)

    # --- Line 16 (Aqua) — Southeast express ---
    s1601 = _st(net, 1601, "Longyang Road",        8.5, -3)     # X: L2, L7, L18, Maglev
    s1602 = _st(net, 1602, "Middle Huaxia Road",   9, -11.5)    # X: L13
    s1603 = _st(net, 1603, "Luoshan Road",         10.5, -14)   # X: L11
    s1604 = _st(net, 1604, "East Zhoupu",          11, -18)
    s1605 = _st(net, 1605, "Heshahangcheng",       11.5, -21)
    s1606 = _st(net, 1606, "East Hangtou",         12, -24)
    s1607 = _st(net, 1607, "Xinchang",             12.5, -27)
    s1608 = _st(net, 1608, "Wild Animal Park",     13, -30)
    s1609 = _st(net, 1609, "Huinan",               13.5, -33)
    s1610 = _st(net, 1610, "East Huinan",          14, -35)
    s1611 = _st(net, 1611, "Shuyuan",              14.5, -37)
    s1612 = _st(net, 1612, "Lingang Avenue",       15, -39)
    s1613 = _st(net, 1613, "Dishui Lake",          15.5, -41)

    # --- Line 17 (Brown) — West from Hongqiao ---
    s1701 = _st(net, 1701, "Hongqiao Railway Stn",  -14, 1)     # X: L2, L10
    s1702 = _st(net, 1702, "Nat. Exhib. & Conv.",   -15, 0.5)
    s1703 = _st(net, 1703, "Panlong Road",          -16, 1)
    s1704 = _st(net, 1704, "Xuying Road",           -17, 2)
    s1705 = _st(net, 1705, "Xujing Beicheng",       -18, 3)
    s1706 = _st(net, 1706, "Middle Jiasong Road",   -19, 4)
    s1707 = _st(net, 1707, "Zhaoxiang",             -20, 5)
    s1708 = _st(net, 1708, "Huijin Road",           -21, 6)
    s1709 = _st(net, 1709, "Qingpu Xincheng",       -22, 7)
    s1710 = _st(net, 1710, "Caoying Road",          -23, 8)
    s1711 = _st(net, 1711, "Dianshanhu Avenue",     -24, 9)
    s1712 = _st(net, 1712, "Zhujiajiao",            -25, 10)
    s1713 = _st(net, 1713, "Oriental Land",         -26, 11.5)
    s1714 = _st(net, 1714, "Xicen",                 -27, 13)

    # --- Line 18 (Champagne) — North-South east side ---
    s1801 = _st(net, 1801, "South Changjiang Road", -0.5, 4.5)  # X: L3
    s1802 = _st(net, 1802, "Yingao Road",           0, 5.5)
    s1803 = _st(net, 1803, "Shanghai Univ. Fin.",   0.5, 6.5)
    s1804 = _st(net, 1804, "Fudan University",      1, 7.5)
    s1805 = _st(net, 1805, "Guoquan Road",          1, 2.2)     # X: L10
    s1806 = _st(net, 1806, "Fushun Road",           0.5, 1.8)
    s1807 = _st(net, 1807, "Jiangpu Road",          1, 2)       # X: L8
    s1808 = _st(net, 1808, "Jiangpu Park",          4, 1)       # X: L12
    s1809 = _st(net, 1809, "Pingliang Road",        4.5, 0.5)
    s1810 = _st(net, 1810, "Danyang Road",          4.8, 0)
    s1811 = _st(net, 1811, "Changyi Road",          5, 0)       # X: L14
    s1812 = _st(net, 1812, "Minsheng Road",        4.4, -2.2)   # X: L6
    s1813 = _st(net, 1813, "Middle Yanggao Road",   6, -2.5)    # X: L9
    s1814 = _st(net, 1814, "Yingchun Road",         7, -2.5)
    s1815 = _st(net, 1815, "Longyang Road",         8.5, -3)    # X: L2, L7, L16, Maglev
    s1816 = _st(net, 1816, "Fangxin Road",          8.5, -4.5)
    s1817 = _st(net, 1817, "Beizhong Road",         9, -6)
    s1818 = _st(net, 1818, "Lianxi Road",           8.5, -10.5) # X: L13
    s1819 = _st(net, 1819, "Yuqiao",                10, -13)    # X: L11
    s1820 = _st(net, 1820, "Kangqiao",              10.5, -14.5)
    s1821 = _st(net, 1821, "Zhoupu",                11, -16)
    s1822 = _st(net, 1822, "Fanrong Road",          11.5, -17.5)
    s1823 = _st(net, 1823, "Shenmei Road",          12, -19)
    s1824 = _st(net, 1824, "Hetao Road",            12.5, -20.5)
    s1825 = _st(net, 1825, "Xiasha",                13, -22)
    s1826 = _st(net, 1826, "Hangtou",               13.5, -23.5)

    # --- Pujiang Line (Grey) — Southern branch from Shendu Highway ---
    s1901 = _st(net, 1901, "Shendu Highway",        11.5, -17)  # X: L8
    s1902 = _st(net, 1902, "Sanlu Highway",         11.5, -18.5)
    s1903 = _st(net, 1903, "Minrui Road",           11.5, -20)
    s1904 = _st(net, 1904, "Puhang Road",           11.5, -21.5)
    s1905 = _st(net, 1905, "Dongchengyi Road",      11.5, -23)
    s1906 = _st(net, 1906, "Huizhen Road",          11.5, -24.5)

    # --- Maglev ---
    s2000 = _st(net, 2000, "Longyang Road",         8.5, -3)    # X: L2, L7, L16, L18
    s2001 = _st(net, 2001, "Pudong Airport T1&T2", 23.5, 0.8)  # X: L2, Airport Link

    # --- Airport Link (Suburban Railway) ---
    s2101 = _st(net, 2101, "Hongqiao Airport T2",  -12.5, 1)    # X: L2, L10
    s2102 = _st(net, 2102, "Zhongchun Road",        -5.5, -5)   # X: L9
    s2103 = _st(net, 2103, "Jinghong Road",          1, -13.5)  # X: L15
    s2104 = _st(net, 2104, "South Sanlin",           6, -11)
    s2105 = _st(net, 2105, "East Kangqiao",          8.5, -12)
    s2106 = _st(net, 2106, "Shanghai Intl. Resort", 11, -10)
    s2107 = _st(net, 2107, "Pudong Airport T1&T2", 23.5, 0.8)  # X: L2, Maglev

    # =====================================================================
    # Lines
    # =====================================================================

    net.add_line(Line(1, "Line 1", [
        s101, s102, s103, s104, s105, s106, s107, s108, s109, s110,
        s111, s112, s113, s114, s115, s116, s117, s118, s119, s120,
        s121, s122, s123, s124, s125, s126, s127, s128,
    ], max_speed=80, color=(220, 40, 40)))

    net.add_line(Line(2, "Line 2", [
        s201, s202, s203, s204, s205, s206, s207, s208, s209, s210,
        s211, s212, s213, s214, s215, s216, s217, s218, s219, s220,
        s221, s222, s223, s224, s225, s226, s227, s228, s229, s230, s231,
    ], max_speed=80, color=(100, 200, 80)))

    net.add_line(Line(3, "Line 3", [
        s301, s302, s303, s304, s305, s306, s307, s308, s309, s310,
        s311, s312, s313, s314, s315, s316, s317, s318, s319, s320,
        s321, s322, s323, s324, s325, s326, s327, s328, s329,
    ], max_speed=80, color=(240, 200, 40)))

    net.add_line(Line(4, "Line 4", [
        s325, s324, s323, s322, s321, s320, s319, s318, s317, s316,
        s416, s415, s414, s413, s412, s411, s410, s409, s408, s407,
        s406, s405, s404, s403, s402, s401, s325,
    ], max_speed=80, color=(100, 50, 160), ring_label_station_id=325))

    net.add_line(Line(5, "Line 5 (Main)", [
        s501, s502, s503, s504, s505, s506, s507, s508, s509, s510,
        s511, s512, s513, s514, s515,
    ], max_speed=80, color=(160, 80, 180)))

    net.add_line(Line(6, "Line 5 (Branch)", [
        s507, s516, s517, s518, s519,
    ], max_speed=60, color=(160, 80, 180)))

    net.add_line(Line(7, "Line 6", [
        s601, s602, s603, s604, s605, s606, s607, s608, s609, s610,
        s611, s612, s613, s614, s615, s616, s617, s618, s619, s620,
        s621, s622, s623, s624, s625, s626, s627, s628,
    ], max_speed=80, color=(220, 80, 130)))

    net.add_line(Line(8, "Line 7", [
        s701, s702, s703, s704, s705, s706, s707, s708, s709, s710,
        s711, s712, s713, s714, s715, s716, s717, s718, s719, s720,
        s721, s722, s723, s724, s725, s726, s727, s728, s729, s730,
        s731, s732, s733,
    ], max_speed=80, color=(240, 140, 40)))

    net.add_line(Line(9, "Line 8", [
        s801, s802, s803, s804, s805, s806, s807, s808, s809, s810,
        s811, s812, s813, s814, s815, s816, s817, s818, s819, s820,
        s821, s822, s823, s824, s825, s826, s827, s828, s829, s830,
    ], max_speed=80, color=(70, 130, 200)))

    net.add_line(Line(10, "Line 9", [
        s901, s902, s903, s904, s905, s906, s907, s908, s909, s910,
        s911, s912, s913, s914, s915, s916, s917, s918, s919, s920,
        s921, s922, s923, s924, s925, s926, s927, s928, s929, s930,
        s931, s932, s933, s934, s935,
    ], max_speed=80, color=(120, 190, 230)))

    net.add_line(Line(11, "Line 10 (Main)", [
        s1001, s1002, s1003, s1004, s1005, s1006, s1007, s1008, s1009,
        s1010, s1011, s1012, s1013, s1014, s1015, s1016, s1017, s1018,
        s1019, s1020, s1021, s1022, s1023, s1024, s1025, s1026, s1027,
        s1028, s1029, s1030, s1031, s1032, s1033, s1034,
    ], max_speed=80, color=(180, 140, 210)))

    net.add_line(Line(12, "Line 10 (Branch)", [
        s1005, s1035, s1036, s1037,
    ], max_speed=60, color=(180, 140, 210)))

    net.add_line(Line(13, "Line 11 (Main)", [
        s1101, s1102, s1103, s1104, s1105, s1106, s1107, s1108, s1109,
        s1110, s1111, s1112, s1113, s1114, s1115, s1116, s1117, s1118,
        s1119, s1120, s1121, s1122, s1123, s1124, s1125, s1126, s1127,
        s1128, s1129, s1130, s1131, s1132, s1133,
    ], max_speed=100, color=(140, 30, 50)))

    net.add_line(Line(14, "Line 11 (Kunshan)", [
        s1104, s1134, s1135, s1136, s1137, s1138, s1139, s1140,
    ], max_speed=100, color=(140, 30, 50)))

    net.add_line(Line(15, "Line 12", [
        s1201, s1202, s1203, s1204, s1205, s1206, s1207, s1208, s1209,
        s1210, s1211, s1212, s1213, s1214, s1215, s1216, s1217, s1218,
        s1219, s1220, s1221, s1222, s1223, s1224, s1225, s1226, s1227,
        s1228, s1229, s1230, s1231, s1232,
    ], max_speed=80, color=(40, 130, 80)))

    net.add_line(Line(16, "Line 13", [
        s1301, s1302, s1303, s1304, s1305, s1306, s1307, s1308, s1309,
        s1310, s1311, s1312, s1313, s1314, s1315, s1316, s1317, s1318,
        s1319, s1320, s1321, s1322, s1323, s1324, s1325, s1326, s1327,
        s1328, s1329, s1330, s1331,
    ], max_speed=80, color=(240, 150, 180)))

    net.add_line(Line(17, "Line 14", [
        s1401, s1402, s1403, s1404, s1405, s1406, s1407, s1408, s1409,
        s1410, s1411, s1412, s1413, s1414, s1415, s1416, s1417, s1418,
        s1419, s1420, s1421, s1422, s1423, s1424, s1425, s1426, s1427,
        s1428, s1429, s1430,
    ], max_speed=80, color=(150, 180, 80)))

    net.add_line(Line(18, "Line 15", [
        s1501, s1502, s1503, s1504, s1505, s1506, s1507, s1508, s1509,
        s1510, s1511, s1512, s1513, s1514, s1515, s1516, s1517, s1518,
        s1519, s1520, s1521, s1522, s1523, s1524, s1525, s1526, s1527,
        s1528, s1529, s1530,
    ], max_speed=80, color=(200, 140, 80)))

    net.add_line(Line(19, "Line 16", [
        s1601, s1602, s1603, s1604, s1605, s1606, s1607, s1608, s1609,
        s1610, s1611, s1612, s1613,
    ], max_speed=120, color=(80, 200, 210)))

    net.add_line(Line(20, "Line 17", [
        s1701, s1702, s1703, s1704, s1705, s1706, s1707, s1708, s1709,
        s1710, s1711, s1712, s1713, s1714,
    ], max_speed=100, color=(180, 120, 80)))

    net.add_line(Line(21, "Line 18", [
        s1801, s1802, s1803, s1804, s1805, s1806, s1807, s1808, s1809,
        s1810, s1811, s1812, s1813, s1814, s1815, s1816, s1817, s1818,
        s1819, s1820, s1821, s1822, s1823, s1824, s1825, s1826,
    ], max_speed=80, color=(210, 190, 160)))

    net.add_line(Line(22, "Pujiang Line", [
        s1901, s1902, s1903, s1904, s1905, s1906,
    ], max_speed=60, color=(150, 150, 155)))

    net.add_line(Line(23, "Maglev", [
        s2000, s2001,
    ], max_speed=430, color=(0, 180, 190)))

    net.add_line(Line(24, "Airport Link", [
        s2101, s2102, s2103, s2104, s2105, s2106, s2107,
    ], max_speed=160, color=(60, 180, 120)))

    _set_station_types(net)
    return net


def _set_station_types(net: MetroNetwork):
    """Heuristic station type assignment."""
    for sid, st in net.stations.items():
        x, y = st.position[0], st.position[1]
        # Pudong (x>2) more likely elevated; far suburbs → ground
        if abs(x) > 20 or abs(y) > 25:
            st.station_type = StationType.GROUND
        elif abs(x) > 14 or abs(y) > 18:
            if sid % 3 == 0:
                st.station_type = StationType.ELEVATED
            else:
                st.station_type = StationType.GROUND
        elif x > 2 and abs(y) > 8:
            if sid % 4 == 0:
                st.station_type = StationType.ELEVATED
