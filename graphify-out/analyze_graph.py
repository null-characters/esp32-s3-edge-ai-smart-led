#!/usr/bin/env python3
"""
知识图谱自动化分析脚本
演示如何使用 graph.json 进行各种分析任务
"""

import json
from collections import defaultdict
from pathlib import Path


class KnowledgeGraphAnalyzer:
    """知识图谱分析器"""

    # 需要排除的非业务代码目录/文件
    EXCLUDE_PATTERNS = [
        'tests_host',      # 主机测试目录
        'tests/',          # 测试目录
        '/test_',          # 测试文件
        'mock_',           # mock 文件
        '/unit/',          # 单元测试
        'mock_esp.h',      # ESP mock 文件
        'test_multimodal', # 测试文件
    ]

    def __init__(self, graph_path: str, exclude_tests: bool = True):
        with open(graph_path, 'r', encoding='utf-8') as f:
            self.graph = json.load(f)

        self.nodes = {n['id']: n for n in self.graph['nodes']}
        self.edges = self.graph.get('links', self.graph.get('edges', []))

        # 过滤非业务代码
        if exclude_tests:
            self._filter_business_code()

    def _filter_business_code(self):
        """过滤掉测试/mock文件，只保留业务代码"""
        # 过滤节点
        filtered_nodes = {}
        for node_id, node in self.nodes.items():
            source_file = node.get('source_file', '')
            should_exclude = any(pattern in source_file for pattern in self.EXCLUDE_PATTERNS)
            if not should_exclude:
                filtered_nodes[node_id] = node

        # 过滤边（只保留两端都是业务代码的边）
        filtered_edges = []
        for edge in self.edges:
            source_id = edge.get('source', '')
            target_id = edge.get('target', '')
            if source_id in filtered_nodes and target_id in filtered_nodes:
                filtered_edges.append(edge)

        self.nodes = filtered_nodes
        self.edges = filtered_edges

        # 重建邻接表
        self.out_edges = defaultdict(list)
        self.in_edges = defaultdict(list)
        for edge in self.edges:
            edge_type = edge.get('relation', edge.get('type', 'unknown'))
            self.out_edges[edge['source']].append((edge['target'], edge_type))
            self.in_edges[edge['target']].append((edge['source'], edge_type))

        print(f"[过滤] 保留 {len(self.nodes)} 个业务节点，{len(self.edges)} 条业务边")

    # ========================================
    # 场景 1: 查找函数调用链
    # ========================================
    def find_call_chain(self, func_name: str, direction: str = 'downstream', max_depth: int = 5):
        """
        查找函数的调用链
        direction: 'upstream' = 谁调用了它, 'downstream' = 它调用了谁
        """
        # 找到匹配的节点
        matches = [n for n in self.nodes.values() if func_name.lower() in n['label'].lower()]

        results = []
        for node in matches:
            chain = self._traverse(node['id'], direction, max_depth)
            results.append({
                'node': node['label'],
                'file': node['source_file'],
                'chain': chain
            })
        return results

    def _traverse(self, node_id: str, direction: str, max_depth: int, visited: set = None, depth: int = 0):
        """递归遍历调用链"""
        if visited is None:
            visited = set()
        if depth >= max_depth or node_id in visited:
            return []

        visited.add(node_id)
        edges = self.in_edges[node_id] if direction == 'upstream' else self.out_edges[node_id]

        chain = []
        for target_id, edge_type in edges:
            if target_id in self.nodes:
                node = self.nodes[target_id]
                chain.append({
                    'name': node['label'],
                    'file': node['source_file'],
                    'type': edge_type,
                    'depth': depth + 1
                })
                # 递归
                sub_chain = self._traverse(target_id, direction, max_depth, visited.copy(), depth + 1)
                chain.extend(sub_chain)
        return chain

    # ========================================
    # 场景 2: 识别重构候选
    # ========================================
    def find_refactoring_candidates(self, threshold: int = 15):
        """
        找出连接数过多的节点（重构候选）
        这些节点可能是"上帝类"或需要拆分的函数
        """
        node_connections = defaultdict(int)

        for edge in self.edges:
            node_connections[edge['source']] += 1
            node_connections[edge['target']] += 1

        candidates = []
        for node_id, count in sorted(node_connections.items(), key=lambda x: -x[1]):
            if count >= threshold and node_id in self.nodes:
                node = self.nodes[node_id]
                candidates.append({
                    'name': node['label'],
                    'file': node['source_file'],
                    'connections': count,
                    'suggestion': self._suggest_refactoring(count)
                })
        return candidates

    def _suggest_refactoring(self, count: int) -> str:
        if count > 50:
            return "严重耦合，建议拆分为多个模块"
        elif count > 30:
            return "高耦合，考虑提取子函数或使用策略模式"
        else:
            return "适度关注，可能需要简化"

    # ========================================
    # 场景 3: 分析社区结构
    # ========================================
    def analyze_communities(self):
        """
        分析社区（聚类）结构
        帮助理解代码的模块划分
        """
        communities = defaultdict(list)
        for node in self.graph['nodes']:
            communities[node['community']].append(node)

        # 计算每个社区的特征
        community_stats = []
        for comm_id, nodes in communities.items():
            # 统计文件类型
            file_types = defaultdict(int)
            source_files = defaultdict(int)
            for n in nodes:
                file_types[n['file_type']] += 1
                source_files[n['source_file'].split('/')[0] if '/' in n['source_file'] else n['source_file']] += 1

            # 找出核心节点（社区内连接最多的）
            internal_connections = defaultdict(int)
            for edge in self.edges:
                source_comm = self.nodes.get(edge['source'], {}).get('community')
                target_comm = self.nodes.get(edge['target'], {}).get('community')
                if source_comm == comm_id:
                    internal_connections[edge['source']] += 1
                if target_comm == comm_id:
                    internal_connections[edge['target']] += 1

            core_nodes = sorted(internal_connections.items(), key=lambda x: -x[1])[:3]

            community_stats.append({
                'id': comm_id,
                'size': len(nodes),
                'file_types': dict(file_types),
                'top_sources': dict(sorted(source_files.items(), key=lambda x: -x[1])[:3]),
                'core_nodes': [{'name': self.nodes[n[0]]['label'], 'connections': n[1]} for n in core_nodes if n[0] in self.nodes]
            })

        return sorted(community_stats, key=lambda x: -x['size'])

    # ========================================
    # 场景 4: 跨版本依赖分析
    # ========================================
    def find_cross_version_dependencies(self):
        """
        找出跨版本（prj-v1, prj-v2, prj-v3）的依赖关系
        帮助理解代码演进
        """
        cross_version = []

        for edge in self.edges:
            source = self.nodes.get(edge['source'])
            target = self.nodes.get(edge['target'])

            if source and target:
                source_version = source['source_file'].split('/')[0] if '/' in source['source_file'] else ''
                target_version = target['source_file'].split('/')[0] if '/' in target['source_file'] else ''

                if source_version != target_version and source_version.startswith('prj-') and target_version.startswith('prj-'):
                    cross_version.append({
                        'source': source['label'],
                        'source_file': source['source_file'],
                        'target': target['label'],
                        'target_file': target['source_file'],
                        'type': edge.get('relation', edge.get('type', 'unknown'))
                    })

        return cross_version

    # ========================================
    # 场景 5: 孤立节点检测
    # ========================================
    def find_orphan_nodes(self):
        """
        找出孤立节点（没有连接的节点）
        可能是未使用的代码或文档缺失
        """
        connected_nodes = set()
        for edge in self.edges:
            connected_nodes.add(edge['source'])
            connected_nodes.add(edge['target'])

        orphans = []
        for node in self.graph['nodes']:
            if node['id'] not in connected_nodes:
                orphans.append({
                    'name': node['label'],
                    'file': node['source_file'],
                    'type': node['file_type']
                })

        return orphans

    # ========================================
    # 场景 6: 搜索路径（最短路径）
    # ========================================
    def find_path_between(self, start_name: str, end_name: str):
        """
        找出两个节点之间的最短路径
        帮助理解代码如何从 A 到达 B
        """
        # 找到起点和终点
        start_nodes = [n for n in self.graph['nodes'] if start_name.lower() in n['label'].lower()]
        end_nodes = [n for n in self.graph['nodes'] if end_name.lower() in n['label'].lower()]

        if not start_nodes or not end_nodes:
            return None

        # BFS 找最短路径
        from collections import deque

        start_id = start_nodes[0]['id']
        end_id = end_nodes[0]['id']

        queue = deque([(start_id, [start_id])])
        visited = {start_id}

        while queue:
            current, path = queue.popleft()

            if current == end_id:
                return [self.nodes[n] for n in path if n in self.nodes]

            for neighbor, _ in self.out_edges[current]:
                if neighbor not in visited:
                    visited.add(neighbor)
                    queue.append((neighbor, path + [neighbor]))

        return None

    # ========================================
    # 场景 7: 生成 AI 上下文
    # ========================================
    def generate_ai_context(self, topic: str, max_nodes: int = 30):
        """
        为 AI 工具生成特定主题的上下文
        提取相关节点和它们的关系
        """
        # 找到相关节点
        relevant_nodes = [n for n in self.graph['nodes']
                         if topic.lower() in n['label'].lower() or
                         topic.lower() in n['source_file'].lower()]

        if not relevant_nodes:
            return None

        # 收集相关边
        relevant_ids = {n['id'] for n in relevant_nodes}
        related_edges = []

        for edge in self.edges:
            if edge['source'] in relevant_ids or edge['target'] in relevant_ids:
                related_edges.append(edge)
                # 添加连接的节点
                if edge['source'] not in relevant_ids and edge['source'] in self.nodes:
                    relevant_nodes.append(self.nodes[edge['source']])
                    relevant_ids.add(edge['source'])
                if edge['target'] not in relevant_ids and edge['target'] in self.nodes:
                    relevant_nodes.append(self.nodes[edge['target']])
                    relevant_ids.add(edge['target'])

                if len(relevant_nodes) >= max_nodes:
                    break

        return {
            'topic': topic,
            'nodes': relevant_nodes[:max_nodes],
            'edges': related_edges,
            'summary': f"找到 {len(relevant_nodes)} 个相关节点，{len(related_edges)} 条关系"
        }


def main():
    """演示各种分析场景"""
    script_dir = Path(__file__).parent
    graph_path = script_dir / 'graph.json'

    analyzer = KnowledgeGraphAnalyzer(str(graph_path))

    print("=" * 60)
    print("📊 知识图谱自动化分析演示")
    print("=" * 60)

    # 场景 1: 查找调用链
    print("\n🔗 场景 1: 查找函数调用链")
    print("-" * 40)
    chains = analyzer.find_call_chain('ai_infer', direction='upstream', max_depth=2)
    for chain in chains:
        print(f"函数: {chain['node']} ({chain['file']})")
        for item in chain['chain'][:5]:
            print(f"  {'  ' * item['depth']}↑ {item['name']} ({item['type']})")

    # 场景 2: 重构候选
    print("\n🔧 场景 2: 识别重构候选")
    print("-" * 40)
    candidates = analyzer.find_refactoring_candidates(threshold=15)
    for c in candidates[:5]:
        print(f"  {c['name']}: {c['connections']} 连接 - {c['suggestion']}")

    # 场景 3: 社区分析
    print("\n🏘️ 场景 3: 社区结构分析 (Top 5)")
    print("-" * 40)
    communities = analyzer.analyze_communities()
    for comm in communities[:5]:
        print(f"  社区 {comm['id']}: {comm['size']} 个节点")
        print(f"    主要来源: {comm['top_sources']}")
        if comm['core_nodes']:
            print(f"    核心节点: {', '.join(n['name'] for n in comm['core_nodes'][:2])}")

    # 场景 4: 跨版本依赖
    print("\n🔄 场景 4: 跨版本依赖分析")
    print("-" * 40)
    cross_deps = analyzer.find_cross_version_dependencies()
    print(f"  发现 {len(cross_deps)} 个跨版本连接")
    for dep in cross_deps[:3]:
        print(f"  {dep['source']} ({dep['source_file'].split('/')[0]}) → {dep['target']} ({dep['target_file'].split('/')[0]})")

    # 场景 5: 孤立节点
    print("\n🏝️ 场景 5: 孤立节点检测")
    print("-" * 40)
    orphans = analyzer.find_orphan_nodes()
    print(f"  发现 {len(orphans)} 个孤立节点")
    for o in orphans[:5]:
        print(f"  {o['name']} ({o['file']})")

    # 场景 6: 路径搜索
    print("\n🛤️ 场景 6: 路径搜索")
    print("-" * 40)
    path = analyzer.find_path_between('main', 'ai_infer')
    if path:
        print(f"  从 main 到 ai_infer 的路径:")
        for i, node in enumerate(path):
            print(f"    {'→' if i > 0 else ' '} {node['label']} ({node['source_file']})")
    else:
        print("  未找到路径")

    # 场景 7: AI 上下文生成
    print("\n🤖 场景 7: AI 上下文生成")
    print("-" * 40)
    context = analyzer.generate_ai_context('multimodal', max_nodes=15)
    if context:
        print(f"  主题: {context['topic']}")
        print(f"  {context['summary']}")
        print(f"  节点列表:")
        for node in context['nodes'][:8]:
            print(f"    - {node['label']} ({node['source_file']})")


if __name__ == '__main__':
    main()
