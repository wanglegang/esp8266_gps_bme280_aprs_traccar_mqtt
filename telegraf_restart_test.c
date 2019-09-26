配置文件保存修改后，记得要重启 telegraf
# 重启 telegraf 服务，使配置文件生效
service telegraf restart

测试配置是否成功
示例
# 测试 /etc/telegraf/telegraf.conf 配置文件中输入 cpu 配置是否正确
telegraf  -config /etc/telegraf/telegraf.conf -input-filter cpu -test
# 测试 /etc/telegraf/telegraf.conf 输出 influxdb 配置是否正确
telegraf  -config /etc/telegraf/telegraf.conf -output-filter influxdb -test
# 测试 /etc/telegraf/telegraf.d/mysql.conf 输入 cpu 和 输出 influxdb 配置是否正确
telegraf  -config /etc/telegraf/telegraf.d/mysql.conf -input-filter cpu  -output-filter influxdb -test
配置正确的话，会输出当前时间戳的配置指标和值；
配置错误的话，会显示错误信息；
其他测试配置文件方法可以类比以上示例
查看 telegraf 的日志
