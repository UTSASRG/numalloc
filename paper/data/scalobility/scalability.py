import matplotlib.pyplot as plt
import matplotlib
import numpy as np

x = ["1", "2", "4", "8", "16", "32", "64", "128"]
x_position = [0, 1, 2, 3, 4, 5, 6, 7]
# y = [0, 0.2, 0.4, 0.6, 0.8, 1, 1.2]
y_thread_test_linux = [1, 0.953692998, 1.377317576, 10.69996631, 6.110736964, 6.761230573, 7.885164494, 8.032755786]
y_thread_test_numalloc = [1.860809188, 3.789903932, 7.674601257, 16.12874556, 32.96056046, 68.07609859, 112.4159292,
                          141.1444444]
y_thread_test_tcmalloc = [2.002301315, 2.087455221, 2.586326248, 3.066727826, 4.010038513, 4.336382877, 4.415056305,
                          3.754063479]
y_thread_test_tamalloc_numa = [1.392231648, 1.238157433, 1.303593785, 1.295907125, 1.489039972, 1.80471103, 2.054105624,
                               2.106703373]
y_thread_test_jemalloc = [2.483771312, 5.423070355, 10.79452753, 19.97955332, 31.99748111, 40.79319204, 52.75332226,
                          71.20515694]
y_thread_test_tbb = [0.999858321, 0.941102386, 1.243052294, 10.77073088, 6.173697512, 6.857590153, 7.740068243,
                     8.129399718]
y_thread_test_scalloc = [0.834362356, 1.200412013, 2.125171479, 3.486578471, 4.863323124, 5.853917051, 7.168735892,
                         7.984286612]
# ============================


y_cache_scrach_linux = [1, 1.990432802, 0.233050621, 0.322280825, 0.584442512, 0.763477501, 1.459495574, 3.048848569]
y_cache_scrach_numalloc = [0.999199543, 1.993156934, 3.957427537, 7.794826048, 14.83531409, 27.05263154, 42.41747568,
                           47.23243239]
y_cache_scrach_tcmalloc = [0.999542439, 0.124131661, 0.134501124, 0.144930421, 0.18887664, 0.358408532, 0.702863578,
                           1.3217365]
y_cache_scrach_tamalloc_numa = [0.996237601, 1.978713768, 3.904378909, 7.591659428, 14.83531409, 28.18709679,
                                48.27624303, 10.2679201]
y_cache_scrach_jemalloc = [0.99965679, 0.129684323, 0.142498369, 0.141425912, 0.191417118, 0.37061543, 0.702129369,
                           1.341624443]
y_cache_scrach_tbb = [1, 1.991793937, 0.237355354, 0.32073117, 0.584012833, 0.783396091, 1.456090652, 3.131899642]
y_cache_scrach_scalloc = [0.999542439, 1.992248062, 3.952057892, 7.760213143, 15.14384749, 28.55555557, 42.00961535,
                          48.010989]
# ============================


y_cache_thrash_linux = [1, 1.99812198, 3.989208962, 7.973313836, 15.89143898, 31.51878612, 62.00710732, 81.61272222]
y_cache_thrash_numalloc = [0.999931232, 1.999083452, 3.993957151, 7.971856725, 15.87409025, 31.36017254, 61.22385965,
                           106.136253]
y_cache_thrash_tcmalloc = [1.000034388, 0.12497153, 0.129879371, 0.161017888, 0.20756764, 0.365941026, 0.697717567,
                           1.34269049]
y_cache_thrash_tamalloc_numa = [0.999633347, 1.997115715, 3.983926207, 7.947167062, 15.78505518, 31.18084346,
                                60.41828256, 80.18749998]
y_cache_thrash_jemalloc = [1.000022925, 1.998167743, 3.988297143, 7.962398467, 15.83663097, 31.11412268, 58.63172042,
                           76.52982457]
y_cache_thrash_tbb = [1.000011462, 1.998396592, 3.989938718, 7.969672056, 15.88275988, 31.51878613, 61.8751773,
                      80.04036696]
y_cache_thrash_scalloc = [0.999954154, 1.998854446, 3.98738574, 7.965306309, 15.86831575, 31.43927928, 60.75487465,
                          79.31272727]
# ============================


y_larson_linux = [1, 1.000223949, 1.000096809, 0.99978218, 0.999926765, 10.65428552, 16.96427947, 21.78747892]
y_larson_numalloc = [2.146170013, 2.147439527, 2.145886344, 2.145614463, 2.145346824, 18.35695356, 34.29835631,
                     54.25454722]
y_larson_tcmalloc = [2.652626102, 2.65283182, 2.653226127, 2.652355006, 2.651933982, 20.1001821, 39.4206086,
                     35.79640057]
y_larson_tamalloc_numa = [2.588409073, 2.578316606, 2.585356769, 2.576599352, 2.584824007, 10.00771892, 2.422149364,
                          0.250100305]
y_larson_jemalloc = [2.47215327, 2.476356748, 2.470211909, 2.476789087, 2.474304592, 20.63733834, 35.82169924,
                     24.35240025]
y_larson_tbb = [0.998726085, 0.998248014, 0.998404856, 0.998073569, 0.999244546, 10.20805357, 16.23218613, 21.81680725]
y_larson_scalloc = [1.701994935, 1.707553579, 1.701574226, 1.708081941, 1.700615883, 28.84705872, 15.42684977,
                    9.384873606]
# ============================

line_marker = ['s', '*', 'v', '+', 'o', '^', 'd']
line_style = ['dotted', 'solid', 'dashed', 'dashdot', '-.', (0, (3, 10, 1, 10, 1, 10)), (0, (3, 1, 1, 1))]
line_color = ['black', 'r', 'chocolate', 'gold', 'lawngreen', 'dodgerblue', 'brown']
label = ["Linux's default", "NUMAlloc", "TcMalloc", "TcMalloc-NUMA", "jemalloc", "TBB", "Scalloc"]
front = "Arial"

# Say, "the default sans-serif font is COMIC SANS"
matplotlib.rcParams['font.sans-serif'] = "Arial"
# Then, "ALWAYS use sans-serif fonts"
matplotlib.rcParams['font.family'] = "sans-serif"

fig, sub_fig = plt.subplots(2, 2, figsize=(10, 10), tight_layout=True)

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
lg = fig.legend(label, bbox_to_anchor=(0.5, 1.01), loc='upper center', ncol=7,
                borderaxespad=0, frameon=False)  # 图例
fig.savefig('test.png', bbox_extra_artists=(lg,), bbox_inches='tight')
# fig.tight_layout()


# lg = sub_fig[0][0].legend(label, bbox_to_anchor=(0.05, 1.05), prop={'size': 6}, loc='upper left', shadow=False, ncol=7,
#                 borderaxespad=0.,)  # 图例
# plt.tight_layout(pad=7)

# handles, labels = sub_fig.get_legend_handles_labels()
# fig.legend(handles, labels, loc='upper center')

# plt.autoscale(False, axis="both", tight=True)
plt.show()
