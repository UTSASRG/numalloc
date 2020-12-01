import matplotlib.pyplot as plt
import matplotlib
import numpy as np

x = ["1", "2", "4", "8", "16", "32", "64", "128"]
x_position = [0, 1, 2, 3, 4, 5, 6, 7]
# y = [0, 0.2, 0.4, 0.6, 0.8, 1, 1.2]
y_thread_test_linux = [1,0.983205988,1.336264994,10.62750716,6.080914264,6.718486947,7.745117307,8.019969473]
y_thread_test_numalloc = [1.877136052,3.65694235,6.885018563,10.53869296,12.56536469,17.31749519,44.2477193,88.55758435]
y_thread_test_tcmalloc = [1.065084459,1.566884521,2.108161423,2.459261282,3.222744697,3.488409405,3.479554108,2.700689596]
y_thread_test_tamalloc_numa = [1.373584001,1.28926921,1.364399628,1.320951962,1.493651395,1.791024002,2.087294756,2.081369248]
y_thread_test_jemalloc = [2.511371331,5.22221302,10.16655917,19.63656182,31.58967936,40.47047496,49.6480315,70.76655443]
y_thread_test_tbb = [1.001318088,0.981033732,1.330512766,10.64365294,6.090311987,6.746522576,7.661360874,7.957218576]
y_thread_test_scalloc = [0.789494772,1.197747089,2.074453035,3.455716321,4.757280821,5.860488893,6.836495717,7.435495283]
y_thread_test_mimalloc = [2.176342676,4.084537151,6.533989637,13.30793584,29.19120371,47.83990895,61.93811395,64.53735927]
# ============================


y_cache_scrach_linux = [1,1.997174737,3.989213515,7.964204221,15.86505682,31.42656162,59.9838883,78.65492956]
y_cache_scrach_numalloc = [0.999373658,1.996532123,3.985512418,7.941552901,15.79773692,30.44983643,57.33572896,96.61764704]
y_cache_scrach_tcmalloc = [0.999856767,1.99667489,3.984659294,7.958529286,15.84255319,31.3735955,60.37297297,79.89270388]
y_cache_scrach_tamalloc_numa = [0.999391542,1.99510557,3.980399145,7.92914951,15.72212838,30.88772124,58.78421053,76.60493828]
y_cache_scrach_jemalloc = [0.999928378,1.997103315,3.984375,7.96079829,15.84255319,31.40888639,60.50379198,79.43812232]
y_cache_scrach_tbb = [0.999946283,1.997246164,3.988928571,7.964204221,15.85154698,31.3735955,60.17780172,78.54430376]
y_cache_scrach_scalloc = [0.999928378,1.998318185,3.990353698,7.960798289,15.84704881,31.39123102,59.2834395,77.56250001]
y_cache_scrach_mimalloc = [0.999982094,1.997103315,3.983238231,7.966476462,15.85154698,31.44425676,60.30777537,78.87711866]
# ============================


y_cache_thrash_linux = [1,1.996675366,3.983808845,7.961938703,15.85381777,31.44876126,59.41808512,80.13342895]
y_cache_thrash_numalloc = [0.999373747,1.996461253,3.985798901,7.940432187,15.79553167,30.47081287,57.99896156,95.14991486]
y_cache_thrash_tcmalloc = [0.999874687,1.996603989,3.987506247,7.957401339,15.84032898,31.34287318,58.79263158,80.36402879]
y_cache_thrash_tamalloc_numa = [0.999373747,1.994821244,3.97870067,7.929159568,15.71995497,30.85801105,57.69938017,76.93250687]
y_cache_thrash_jemalloc = [0.999910487,1.99617584,3.984661482,7.958535195,15.8448227,31.41338582,59.41808509,79.90414885]
y_cache_thrash_tbb = [0.999910487,1.996889525,3.984377229,7.959669374,15.85831914,31.43106359,58.97888066,79.4495021]
y_cache_thrash_scalloc = [0.999892587,1.998175444,3.989784985,7.958535195,15.84032898,31.39572793,59.41808509,79.67617683]
y_cache_thrash_mimalloc = [0.99994629,1.996889525,3.983808845,7.960803877,15.85381777,31.43106359,59.41808512,80.595959]
# ============================


y_larson_linux = [1,1.471114923,1.790086381,3.418296867,6.558663334,11.96806837,20.19142148,30.23730298]
y_larson_numalloc = [2.174232114,4.055367143,5.670264233,11.01536637,23.09051843,28.40293129,44.47454664,47.63343634]
y_larson_tcmalloc = [1.005654399,1.455200735,1.472411195,2.981901833,5.949679802,11.42874825,21.20278817,14.88738324]
y_larson_tamalloc_numa = [2.613481231,2.539973098,2.952231744,4.402300681,6.392049298,10.07513161,2.433160777,0.2553929]
y_larson_jemalloc = [2.577087434,2.754709078,3.017419976,5.705420442,11.16488097,21.06088482,36.12180898,23.15107209]
y_larson_tbb = [0.999311816,1.563619082,1.709993001,3.233066027,6.108546975,11.10159091,16.63064601,26.74794064]
y_larson_scalloc = [1.779194871,2.647318328,5.310868211,10.30723839,19.80521928,34.74292241,14.81877519,8.947809224]
y_larson_mimalloc = [2.299448232,2.500201982,2.940716761,5.320713725,11.21720164,21.20748555,38.28843111,66.32889168]
# ============================

line_marker = ['s', '*', 'v', '+', 'o', '^', 'd', 'x']
line_style = ['dotted', 'solid', 'dashed', 'dashdot', '-.', (0, (3, 10, 1, 10, 1, 10)), (0, (3, 1, 1, 1)), 'dotted']
line_color = ['black', 'r', 'chocolate', 'gold', 'lawngreen', 'dodgerblue', 'brown', 'm']
label = ["Linux's default", "NUMAlloc", "TcMalloc", "TcMalloc-NUMA", "jemalloc", "TBB", "Scalloc", "mimalloc"]
front = "Arial"

# Say, "the default sans-serif font is COMIC SANS"
matplotlib.rcParams['font.sans-serif'] = "Arial"
# Then, "ALWAYS use sans-serif fonts"
matplotlib.rcParams['font.family'] = "sans-serif"

fig, sub_fig = plt.subplots(2, 2, figsize=(11, 8), tight_layout=True)

sub_fig[1][1].plot(x, y_thread_test_linux, line_marker[0], color=line_color[0], linestyle=line_style[0], label=label[0])
sub_fig[1][1].plot(x, y_thread_test_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[1][1].plot(x, y_thread_test_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[1][1].plot(x, y_thread_test_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[1][1].plot(x, y_thread_test_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[1][1].plot(x, y_thread_test_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[1][1].plot(x, y_thread_test_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[1][1].plot(x, y_thread_test_mimalloc, line_marker[7], color=line_color[7], linestyle=line_style[7],
                   label=label[7])
sub_fig[1][1].set_xticks(x_position)
sub_fig[1][1].set_xticklabels(x)
# sub_fig[1][1].set_xscale(1, 'linear')
# sub_fig[0][0].set_yticks(y)
sub_fig[1][1].set_xlabel("Number Of Threads", fontname=front)
sub_fig[1][1].set_ylabel("Speedup with respect to the default Linux allocator", fontname=front)
sub_fig[1][1].set_ylim(bottom=0)
# sub_fig[1][1].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator")
# sub_fig[0][0].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator", ylim=[y[0], y[-1]])
sub_fig[1][1].set_title("(d) thread-test", fontname=front)

# plt.legend(label, bbox_to_anchor=(0.05, 1.05), prop={'size': 6}, loc='upper left', shadow=False, ncol=7,
#                 borderaxespad=0.,)
# ============================

sub_fig[0][0].plot(x, y_cache_scrach_linux, line_marker[0], color=line_color[0], linestyle=line_style[0],
                   label=label[0])
sub_fig[0][0].plot(x, y_cache_scrach_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[0][0].plot(x, y_cache_scrach_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[0][0].plot(x, y_cache_scrach_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[0][0].plot(x, y_cache_scrach_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[0][0].plot(x, y_cache_scrach_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[0][0].plot(x, y_cache_scrach_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[0][0].plot(x, y_cache_scrach_mimalloc, line_marker[7], color=line_color[7], linestyle=line_style[7],
                   label=label[7])
sub_fig[0][0].set_xticks(x_position)
sub_fig[0][0].set_xticklabels(x)

# sub_fig[0][1].set_yticks(y)
sub_fig[0][0].set_xlabel("Number Of Threads", fontname=front)
sub_fig[0][0].set_ylabel("Speedup with respect to the default Linux allocator", fontname=front)
sub_fig[0][0].set_ylim(bottom=0)
# sub_fig[0][0].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator")
# sub_fig[0][1].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator", ylim=[y[0], y[-1]])
sub_fig[0][0].set_title("(a) cache-scratch", fontname=front)

# ============================


sub_fig[0][1].plot(x, y_cache_thrash_linux, line_marker[0], color=line_color[0], linestyle=line_style[0],
                   label=label[0])
sub_fig[0][1].plot(x, y_cache_thrash_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[0][1].plot(x, y_cache_thrash_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[0][1].plot(x, y_cache_thrash_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[0][1].plot(x, y_cache_thrash_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[0][1].plot(x, y_cache_thrash_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[0][1].plot(x, y_cache_thrash_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[0][1].plot(x, y_cache_thrash_mimalloc, line_marker[7], color=line_color[7], linestyle=line_style[7],
                   label=label[7])
sub_fig[0][1].set_xticks(x_position)
sub_fig[0][1].set_xticklabels(x)

# sub_fig[1][0].set_yticks(y)
# sub_fig[0][1].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator")
sub_fig[0][1].set_xlabel("Number Of Threads", fontname=front)
sub_fig[0][1].set_ylabel("Speedup with respect to the default Linux allocator", fontname=front)
sub_fig[0][1].set_ylim(bottom=0)
# sub_fig[1][0].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator", ylim=[y[0], y[-1]])
sub_fig[0][1].set_title("(b) cache-thrash", fontname=front)

# ============================

sub_fig[1][0].plot(x, y_larson_linux, line_marker[0], color=line_color[0], linestyle=line_style[0], label=label[0])
sub_fig[1][0].plot(x, y_larson_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[1][0].plot(x, y_larson_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[1][0].plot(x, y_larson_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[1][0].plot(x, y_larson_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[1][0].plot(x, y_larson_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[1][0].plot(x, y_larson_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[1][0].plot(x, y_larson_mimalloc, line_marker[7], color=line_color[7], linestyle=line_style[7],
                   label=label[7])
sub_fig[1][0].set_xticks(x_position)
sub_fig[1][0].set_xticklabels(x)

# sub_fig[1][1].set_yticks(y)
# sub_fig[1][1].ylim(y[-1])
# sub_fig[1][0].autoscale(False)
# sub_fig[1][0].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator")
sub_fig[1][0].set_xlabel("Number Of Threads", fontname=front)
sub_fig[1][0].set_ylabel("Speedup with respect to the default Linux allocator", fontname=front)
sub_fig[1][0].set_ylim(bottom=0)
# sub_fig[1][1].set(xlabel='Number Of Threads', ylabel="Speedup with respect to the default Linux allocator", ylim=[y[0], y[-1]])
sub_fig[1][0].set_title("(c) larson", fontname=front)

# ============================
lg = fig.legend(label, bbox_to_anchor=(0.5, 1), loc='upper center', ncol=8,fancybox=True,shadow=True,
                borderaxespad=-0.5, frameon=False)  # 图例
fig.savefig('test.png', bbox_extra_artists=(lg,), bbox_inches='tight')
# fig.tight_layout()


# lg = sub_fig[0][0].legend(label, bbox_to_anchor=(0.05, 1.05), prop={'size': 6}, loc='upper left', shadow=False, ncol=7,
#                 borderaxespad=0.,)  # 图例
# plt.tight_layout(pad=7)

# handles, labels = sub_fig.get_legend_handles_labels()
# fig.legend(handles, labels, loc='upper center')

# plt.autoscale(False, axis="both", tight=True)
plt.show()
