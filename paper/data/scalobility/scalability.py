import matplotlib.pyplot as plt
import numpy as np

x = [2, 4, 8, 16, 32, 64, 128]
y = [0, 0.2, 0.4, 0.6, 0.8, 1, 1.2]
y_thread_test_linux = [1.048555459, 0.726048965, 0.093458238, 0.163646383, 0.14790207, 0.126820436, 0.124490278]
y_thread_test_numalloc = [0.490991123, 0.242463305, 0.11537222, 0.056455629, 0.027334251, 0.016552896, 0.013183722]
y_thread_test_tcmalloc = [0.959206835, 0.774187447, 0.652911321, 0.499322216, 0.461744586, 0.453516598, 0.533369061]
y_thread_test_tamalloc_numa = [1.124438307, 1.067995002, 1.074329804, 0.934986081, 0.771442976, 0.67777997, 0.660857938]
y_thread_test_jemalloc = [0.458000939, 0.230095417, 0.124315658, 0.077623964, 0.060886908, 0.047082747, 0.034881902]
y_thread_test_tbb = [1.062433096, 0.804357408, 0.092831056, 0.161954537, 0.145803161, 0.129179523, 0.122992885]
y_thread_test_scalloc = [0.695063318, 0.392609427, 0.23930692, 0.171562188, 0.142530608, 0.116389049, 0.104500552]
# ============================


y_cache_scrach_linux = [0.502403296, 4.290913252, 3.102883955, 1.711032273, 1.309796292, 0.685168231, 0.327992676]
y_cache_scrach_numalloc = [0.501315037, 0.252487135, 0.128187536, 0.067352773, 0.036935392, 0.023556318, 0.021154946]
y_cache_scrach_tcmalloc = [8.052276367, 7.431480211, 6.89670556, 5.292038435, 2.788835507, 1.422100206, 0.756234271]
y_cache_scrach_tamalloc_numa = [0.503477369, 0.255159047, 0.13122791, 0.067153118, 0.035343746, 0.020636187,
                                0.097024285]
y_cache_scrach_jemalloc = [7.708385769, 7.015215651, 7.068413225, 5.222400183, 2.69728864, 1.423750143, 0.745109255]
y_cache_scrach_tbb = [0.502059968, 4.213092241, 3.117875944, 1.712291142, 1.276493477, 0.686770428, 0.319295033]
y_cache_scrach_scalloc = [0.501715854, 0.252916953, 0.128803477, 0.066003203, 0.035003432, 0.023793182, 0.020819035]
# ============================


y_cache_thrash_linux = [0.500469946, 0.250676264, 0.125418367, 0.062926963, 0.03172711, 0.016127184, 0.012252992]
y_cache_thrash_numalloc = [0.500194842, 0.250361032, 0.125432665, 0.062991404, 0.031885387, 0.016332378, 0.009421203]
y_cache_thrash_tcmalloc = [8.002097638, 7.699716876, 6.210703683, 4.817872331, 2.732774727, 1.433293979, 0.74479889]
y_cache_thrash_tamalloc_numa = [0.500538521, 0.250916632, 0.125784866, 0.063327834, 0.032059214, 0.016545213,
                                0.012466199]
y_cache_thrash_jemalloc = [0.500469957, 0.250739323, 0.125593178, 0.063146191, 0.032140483, 0.017056005, 0.013067101]
y_cache_thrash_tbb = [0.500406909, 0.250633289, 0.125477116, 0.062962071, 0.031727474, 0.016161755, 0.012493839]
y_cache_thrash_scalloc = [0.500263616, 0.250779387, 0.125538694, 0.063015771, 0.031805887, 0.01645883, 0.012607739]
# ============================


y_larson_linux = [0.999776101, 0.999903201, 1.000217867, 1.000073241, 0.093858945, 0.05894739, 0.045897922]
y_larson_numalloc = [0.999408824, 1.000132192, 1.000258924, 1.000383709, 0.116913191, 0.062573553, 0.039557422]
y_larson_tcmalloc = [0.999922453, 0.999773851, 1.000102209, 1.000260987, 0.131970252, 0.067290339, 0.074103152]
y_larson_tamalloc_numa = [1.003914363, 1.001180612, 1.004583453, 1.001386967, 0.258641264, 1.068641394, 10.34948385]
y_larson_jemalloc = [0.998302555, 1.000785909, 0.998128295, 0.999130535, 0.119790315, 0.06901273, 0.101515795]
y_larson_tbb = [1.000478911, 1.000321742, 1.000653775, 0.999481147, 0.097837073, 0.061527516, 0.04577783]
y_larson_scalloc = [0.996744674, 1.000247247, 0.99643635, 1.000810914, 0.05900064, 0.110326798, 0.181355126]
# ============================

line_marker = ['s', '*', 'v', '+', 'o', '^', 'd']
line_style = ['dotted', 'solid', 'dashed', 'dashdot', '-.', (0, (3, 10, 1, 10, 1, 10)), (0, (3, 1, 1, 1))]
line_color = ['black', 'r', 'chocolate', 'gold', 'lawngreen', 'dodgerblue', 'brown']
label = ["Linux's default", "NUMAlloc", "TcMalloc", "TcMalloc-NUMA", "jemalloc", "TBB", "Scalloc"]

fig, sub_fig = plt.subplots(2, 2, figsize=(10, 10), tight_layout=True)

sub_fig[0][0].plot(x, y_thread_test_linux, line_marker[0], color=line_color[0], linestyle=line_style[0], label=label[0])
sub_fig[0][0].plot(x, y_thread_test_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[0][0].plot(x, y_thread_test_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[0][0].plot(x, y_thread_test_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[0][0].plot(x, y_thread_test_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[0][0].plot(x, y_thread_test_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[0][0].plot(x, y_thread_test_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[0][0].set_xticks(x)
sub_fig[0][0].set_yticks(y)
sub_fig[0][0].set(xlabel='number of threads', ylabel="Normalized Runtime", ylim=[y[0], y[-1]])
sub_fig[0][0].set_title("(a) thread-test")

# plt.legend(label, bbox_to_anchor=(0.05, 1.05), prop={'size': 6}, loc='upper left', shadow=False, ncol=7,
#                 borderaxespad=0.,)
# ============================

sub_fig[0][1].plot(x, y_cache_scrach_linux, line_marker[0], color=line_color[0], linestyle=line_style[0],
                   label=label[0])
sub_fig[0][1].plot(x, y_cache_scrach_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[0][1].plot(x, y_cache_scrach_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[0][1].plot(x, y_cache_scrach_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[0][1].plot(x, y_cache_scrach_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[0][1].plot(x, y_cache_scrach_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[0][1].plot(x, y_cache_scrach_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[0][1].set_xticks(x)
sub_fig[0][1].set_yticks(y)
sub_fig[0][1].set(xlabel='number of threads', ylabel="Normalized Runtime", ylim=[y[0], y[-1]])
sub_fig[0][1].set_title("(b) cache-scrach")

# ============================


sub_fig[1][0].plot(x, y_cache_thrash_linux, line_marker[0], color=line_color[0], linestyle=line_style[0],
                   label=label[0])
sub_fig[1][0].plot(x, y_cache_thrash_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[1][0].plot(x, y_cache_thrash_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[1][0].plot(x, y_cache_thrash_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[1][0].plot(x, y_cache_thrash_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[1][0].plot(x, y_cache_thrash_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[1][0].plot(x, y_cache_thrash_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[1][0].set_xticks(x)
sub_fig[1][0].set_yticks(y)
sub_fig[1][0].set(xlabel='number of threads', ylabel="Normalized Runtime", ylim=[y[0], y[-1]])
sub_fig[1][0].set_title("(c) cache-thrash")

# ============================

sub_fig[1][1].plot(x, y_larson_linux, line_marker[0], color=line_color[0], linestyle=line_style[0], label=label[0])
sub_fig[1][1].plot(x, y_larson_numalloc, line_marker[1], color=line_color[1], linestyle=line_style[1],
                   label=label[1])
sub_fig[1][1].plot(x, y_larson_tcmalloc, line_marker[2], color=line_color[2], linestyle=line_style[2],
                   label=label[2])
sub_fig[1][1].plot(x, y_larson_tamalloc_numa, line_marker[3], color=line_color[3], linestyle=line_style[3],
                   label=label[3])
sub_fig[1][1].plot(x, y_larson_jemalloc, line_marker[4], color=line_color[4], linestyle=line_style[4],
                   label=label[4])
sub_fig[1][1].plot(x, y_larson_tbb, line_marker[5], color=line_color[5], linestyle=line_style[5], label=label[5])
sub_fig[1][1].plot(x, y_larson_scalloc, line_marker[6], color=line_color[6], linestyle=line_style[6],
                   label=label[6])
sub_fig[1][1].set_xticks(x)
sub_fig[1][1].set_yticks(y)
# sub_fig[1][1].ylim(y[-1])
sub_fig[1][1].autoscale(False)
sub_fig[1][1].set(xlabel='number of threads', ylabel="Normalized Runtime", ylim=[y[0], y[-1]])
sub_fig[1][1].set_title("(d) larson")

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
