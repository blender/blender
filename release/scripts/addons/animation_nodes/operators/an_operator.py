class AnimationNodeOperator:
    @classmethod
    def poll(cls, context):
        if not hasattr(context, "active_node"): return False
        return getattr(context.active_node, "isAnimationNode", False)
