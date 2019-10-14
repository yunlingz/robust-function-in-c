/**
 * Copyright 2018 靈 <meetchuling@outlook.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * 如何写出对安全性要求极高的C函数代码
 * 首先请注意, 要写出这种函数, 就尽量不要使用递归
 * 因为嵌入式系统的stack空间非常诡异(大雾)
 * 而递归函数可以神出鬼没地爆掉嵌入式系统的stack空间
 * 但是这类递归函数在逻辑上又没有问题, 所以非常难调试
 */

#define FAILURE -1
#define SUCCESS 0

/**
 * 这个函数适用于你需要顺序完成所有可能发生错误的步骤并完成计算,
 * 计算成功后**不需要**回滚前序步骤的情况 也就是说, 该类型函数的执行结果只有三种
 * 1. 函数参数合法, 前序步骤全部正确执行, 核心计算成功完成, 函数返回值表示成功
 * 2. 函数参数合法, 前序步骤中途出错, 核心计算未执行, 函数自动回滚已执行的前序步骤, 返回值表示失败
 * 3. 函数参数不合法, 函数直接返回, 返回值表示失败
 * 比如说工厂函数就属于这一类
 * 这类函数的典型目的就是**保证产生新的, 正确的持久性作用**
 */
int
robust_function_get_everything_done_but_not_undone(int var_1, int var_2, int var_3)
{
	/**
	 * 首先, 将函数的返回值初始化为约定的失败标志
	 * 比如返回int的可以约定为-1, 返回指针的约定为NULL
	 */
	int result = FAILURE;

	/* 开始对所有的参数的合法性检查, 只要有一个不合法, 则直接跳转到out域 */
	if (!var_1_is_valid(var_1) || !var_2_is_valid(var_2) || !var_3_is_valid(var_3)) {
		goto out;
	}
	/**
	 * do_a, do_b, do_c...等都指代的是可能失败, 或者产生副作用的步骤
	 * 比如申请堆内存, 读写文件, 系统调用等等
	 * 只要某个步骤可能失败, 那么它就必须严格按照发生的顺序被包含在do_a, do_b, do_c...里面
	 * 切记, do_a, do_b, do_c...一定要严格按照发生的顺序依次排列执行
	 */
	if (!do_a()) {
		/**
		 * 以do_a为例
		 * 首先执行do_a, 假如其失败了, 则进入if分支
		 * 由于do_a是第一个可能发生失败的步骤, 也就是说在它前面没有步骤可能失败,
		 * 所以直接跳转到out域 注意, do_a并不一定非要在if的条件部分执行
		 * 比如说你可以在这个if语句前面malloc一块内存,
		 * 然后在if语句的判断部分判定指针是否为NULL来检测失败
		 */
		goto out;
	}
	if (!do_b()) {
		/**
		 * 以do_b为例
		 * 首先执行do_b, 假如其失败了, 则进入if分支
		 * 从第二个可能发生失败的步骤开始, 每当其失败, 就跳转到undo上一个步骤的代码域
		 * do_b的上一个可能失败的步骤是do_a(但是注意do_a已经被正确执行了), 当do_b失败后,
		 * 就跳转到cleanup_a域 这样做的理由, 是确保在本步骤失败的情况下,
		 * 函数能够逆向清理掉前面的步骤所造成的影响 一个健壮的函数, 在其失败的情况下,
		 * 必须要保证其不产生副作用
		 */
		goto cleanup_a;
	}
	if (!do_c()) {
		goto cleanup_b;
	}
	if (!do_d()) {
		goto cleanup_c;
	}

	/* 在所有可能有危险的步骤都顺利执行完成后, 现在开始完成本函数的核心任务 */

	/**
	 * 核心任务执行完成后, 将result置为约定的成功标志
	 * 然后直接跳转到out域
	 */
	result = SUCCESS;
	goto out;
	/**
	 * 注意下面的cleanup_c, cleanup_b, cleanup_a是严格逆向排序的
	 * 这是为了能严格按照do_a, do_b, do_c发生的顺序来逆向解除影响, 否则可能造成致命性错误
	 * 此外注意这里没有cleanup_d, 也就是说没有最后一个可能失败的步骤的undo过程
	 * 这是因为
	 * 如果最后一个步骤成功了, 那么没有必要undo它
	 * 如果其失败了, 其根本没有发生, 也没有必要undo它
	 * 所以在这里没有cleanup_d的步骤
	 */
cleanup_c:
	undo_c();
cleanup_b:
	undo_b();
cleanup_a:
	undo_a();
	/**
	 * 只有在核心任务成功的情况下result才会是成功
	 * 其它情况下都是失败
	 * *********************************************
	 * 另外, 不建议把函数的计算结果作为返回值, 返回值最好用来表示该函数是否成功执行
	 * 比如说, 你有一个函数要计算两个矩阵dot的结果
	 * 那么你最好把计算的结果用参数列表里的一个指针传出去, 而不是通过返回值的方式
	 * 返回值只用来告诉调用者这个函数执行是否成功
	 * 如果你还是不能理解, 请类比scanf函数的工作方式
	 * *********************************************
	 * 当然, 还有一种做法就是函数返回指向堆内存的指针
	 * 还是假设你有一个函数要计算两个矩阵dot的结果
	 * 你把计算的结果矩阵储存在一个新开辟的堆内存空间里, 然后返回指向该堆内存的合法指针
	 * 用返回NULL的方式代表计算失败
	 * 这样函数的调用者也可以通过检查函数的返回值来判断函数是否正确执行
	 * 如果你还是不能理解, 请类比malloc函数的工作方式
	 * *********************************************
	 * 两种风格各有优点, 但也都不是绝对完美的, 具体情况具体选择
	 */
out:
	return result;
}

/**
 * 这个函数适用于你需要顺序完成所有可能发生错误的步骤并完成计算,
 * 计算成功后**需要**回滚前序步骤的情况 也就是说, 该类型函数的执行结果只有三种
 * 1. 函数参数合法, 前序步骤全部正确执行, 核心计算成功完成, 函数自动回滚所有前序步骤,
 * 函数返回值表示成功
 * 2. 函数参数合法, 前序步骤中途出错, 核心计算未执行, 函数自动回滚已执行的前序步骤, 返回值表示失败
 * 3. 函数参数不合法, 函数直接返回, 返回值表示失败
 * 除了工厂类函数外的绝大部分函数, 都属于这个类型
 * 这类函数的典型目的就是**保证在任何情况下, 函数退出后不产生任何副作用, 同时完成计算任务**
 * 注意这里副作用的意思是: 与核心计算结果无关的作用
 */
int
robust_function_get_everything_done_and_undone(int var_1, int var_2, int var_3)
{
	int result = FAILURE;

	if (!var_1_is_valid(var_1) || !var_2_is_valid(var_2) || !var_3_is_valid(var_3)) {
		goto out;
	}
	if (!do_a()) {
		goto out;
	}
	if (!do_b()) {
		goto cleanup_a;
	}
	if (!do_c()) {
		goto cleanup_b;
	}
	if (!do_d()) {
		goto cleanup_c;
	}
	/* 前面步骤的意义请参考第一个函数内的注释 */

	/* 在所有可能有危险的步骤都顺利执行完成后, 现在开始完成本函数的核心任务 */

	/**
	 * 核心任务执行完成后, 将result置为约定的成功标志
	 * 注意后面和上面那个函数的区别, 现在我们不直接跳转到out域
	 * 而是开始undo最后一个已经执行成功, 但是理论上可能失败的步骤
	 * 本例中即为undo_d()
	 * 这里解释一下为什么不为undo_d设置goto的跳转标号
	 * 因为goto标号是为了中途出错快速回滚而设置的
	 * 而到了函数执行到了这里说明中途没有出错, 随后直接正常回滚最后一个前序步骤即可,
	 * 没有必要设置标号 如果你还是不能理解, 那么你可以试着给undo_d前面加上一个cleanup_d标号
	 * 然后你就会发现这个标号根本用不到
	 */
	result = SUCCESS;
	undo_d();
cleanup_c:
	undo_c();
cleanup_b:
	undo_b();
cleanup_a:
	undo_a();
out:
	return result;
}

/* 上面两种函数的本质区别就在于: 要不要在核心任务执行成功后回滚前序步骤 */
