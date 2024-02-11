#ifndef __TSDB2_COMMON_PREPROCESSOR_H__
#define __TSDB2_COMMON_PREPROCESSOR_H__

#define _TSDB2_PP_STRINGIFY_IMPL(...) #__VA_ARGS__
#define TSDB2_PP_STRINGIFY(...) _TSDB2_PP_STRINGIFY_IMPL(__VA_ARGS__)

#define TSDB2_PP_PRIMITIVE_CAT(A, ...) A##__VA_ARGS__
#define TSDB2_PP_CAT(A, ...) TSDB2_PP_PRIMITIVE_CAT(A, __VA_ARGS__)

#define _TSDB2_PP_IF0(THEN, ELSE) ELSE
#define _TSDB2_PP_IF1(THEN, ELSE) THEN
#define TSDB2_PP_IF(CONDITION, THEN, ELSE) TSDB2_PP_CAT(_TSDB2_PP_IF, CONDITION)(THEN, ELSE)

#define _TSDB2_PP_IDENTITY(...) __VA_ARGS__
#define TSDB2_PP_UNPACK(...) _TSDB2_PP_IDENTITY __VA_ARGS__

#define _TSDB2_PP_101ST(_00, _01, _02, _03, _04, _05, _06, _07, _08, _09, _10, _11, _12, _13, _14, \
                        _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
                        _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, \
                        _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
                        _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, _71, _72, _73, _74, \
                        _75, _76, _77, _78, _79, _80, _81, _82, _83, _84, _85, _86, _87, _88, _89, \
                        _90, _91, _92, _93, _94, _95, _96, _97, _98, _99, _100, x, ...)            \
  x

#define TSDB2_PP_NUM_ARGS(...)                                                                    \
  _TSDB2_PP_101ST(_, ##__VA_ARGS__, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86,  \
                  85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, \
                  65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, \
                  45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, \
                  25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,  \
                  4, 3, 2, 1, 0)

#define _TSDB2_PP_FOR_EACH_0(OP, data, ...)
#define _TSDB2_PP_FOR_EACH_1(OP, data, x) OP(data, x)
#define _TSDB2_PP_FOR_EACH_2(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_1(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_3(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_2(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_4(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_3(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_5(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_4(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_6(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_5(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_7(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_6(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_8(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_7(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_9(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_8(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_10(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_9(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_11(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_10(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_12(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_11(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_13(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_12(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_14(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_13(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_15(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_14(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_16(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_15(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_17(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_16(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_18(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_17(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_19(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_18(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_20(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_19(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_21(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_20(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_22(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_21(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_23(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_22(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_24(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_23(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_25(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_24(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_26(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_25(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_27(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_26(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_28(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_27(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_29(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_28(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_30(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_29(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_31(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_30(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_32(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_31(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_33(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_32(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_34(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_33(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_35(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_34(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_36(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_35(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_37(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_36(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_38(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_37(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_39(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_38(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_40(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_39(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_41(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_40(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_42(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_41(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_43(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_42(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_44(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_43(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_45(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_44(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_46(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_45(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_47(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_46(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_48(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_47(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_49(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_48(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_50(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_49(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_51(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_50(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_52(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_51(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_53(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_52(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_54(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_53(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_55(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_54(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_56(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_55(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_57(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_56(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_58(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_57(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_59(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_58(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_60(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_59(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_61(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_60(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_62(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_61(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_63(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_62(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_64(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_63(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_65(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_64(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_66(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_65(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_67(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_66(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_68(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_67(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_69(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_68(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_70(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_69(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_71(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_70(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_72(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_71(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_73(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_72(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_74(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_73(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_75(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_74(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_76(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_75(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_77(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_76(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_78(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_77(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_79(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_78(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_80(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_79(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_81(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_80(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_82(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_81(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_83(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_82(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_84(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_83(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_85(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_84(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_86(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_85(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_87(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_86(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_88(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_87(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_89(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_88(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_90(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_89(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_91(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_90(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_92(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_91(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_93(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_92(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_94(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_93(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_95(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_94(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_96(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_95(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_97(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_96(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_98(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_97(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_99(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_98(OP, data, __VA_ARGS__)
#define _TSDB2_PP_FOR_EACH_100(OP, data, x, ...) \
  OP(data, x) _TSDB2_PP_FOR_EACH_99(OP, data, __VA_ARGS__)

#define TSDB2_PP_FOR_EACH(OP, data, ...)                            \
  TSDB2_PP_CAT(_TSDB2_PP_FOR_EACH_, TSDB2_PP_NUM_ARGS(__VA_ARGS__)) \
  (OP, data, __VA_ARGS__)

#endif  // __TSDB2_COMMON_PREPROCESSOR_H__
