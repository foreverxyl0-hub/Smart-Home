# -*- coding: utf-8 -*-
"""
实验名称：公共网页接口并发性能压力测试
被测接口：公开免费测试网页 https://httpbin.org/get
测试工具：Leming WebRunner v2社区版
测试用途：课程性能测试实训作业
测试并发梯度：50 / 100 / 200 三组虚拟用户压测
"""
from loguru import logger
from webrunnercore import wr
from webrunnercore import *

# 全局日志初始化，记录请求耗时与返回状态
logger.info("===== 公共网页接口压测脚本加载完成 =====")
logger.info("被测目标地址：https://httpbin.org/get")
logger.info("测试开始时间：2026-07-04 22:30:00")

class 测试负载配置(WebLoadMachine):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # 自定义负载任务名称
        self.负载名称 = "httpbin公共网页GET接口压力负载"
        # 本地测试节点配置
        self.测试机集群 = [
            {
                "ip地址": "127.0.0.1",
                "端口": 50000,
                "节点并发上限": 200,
                "节点数": 1,
                "主节点标识": True
            }
        ]
        logger.debug("负载硬件节点配置加载完毕")

    def on_start(self):
        """测试负载启动前置函数"""
        super().on_start()
        logger.info("压力负载引擎已启动，等待虚拟用户执行任务")

    def on_stop(self):
        """测试负载结束后置函数"""
        logger.info("所有并发任务执行完成，负载引擎关闭")
        super().on_stop()

class 接口测试业务场景(WebScenario):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.场景名称 = "网页基础访问业务场景"

    def on_start(self):
        """单虚拟用户初始化操作"""
        super().on_start()
        logger.debug(f"虚拟用户 {self.user_id} 会话创建成功")

    def on_stop(self):
        """单虚拟用户销毁收尾"""
        logger.debug(f"虚拟用户 {self.user_id} 会话结束，释放资源")
        super().on_stop()

    def task_default(self):
        """核心业务：访问公共网页GET接口"""
        # 请求头统一配置
        headers_info = {
            "Content-Type": "application/json",
            "User-Agent": "WebRunner-PerformanceTest/1.0",
            "Accept": "*/*"
        }
        # 发送GET请求访问公开测试网页
        resp = wr.GET(
            url="https://httpbin.org/get",
            headers=headers_info,
            timeout=5000
        )
        # 校验接口返回状态码，记录测试结果
        if resp.status_code == 200:
            logger.success(f"用户{self.user_id} 接口请求成功，状态码200，响应耗时：{resp.response_time}ms")
        else:
            logger.error(f"用户{self.user_id} 接口访问异常，状态码：{resp.status_code}")
        # 单次请求完成后短暂停顿，模拟真实用户浏览间隔
        wr.sleep(0.2)