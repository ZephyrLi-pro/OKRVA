"""Plain-chat fallback cannot bypass the validated presentation workflow."""
from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'src'))

from riscv_ai_adapter.backends import rewrite_chat_request


class PresentationFallbackTests(unittest.TestCase):
    NAMES=('ppt__open_presentation', 'ppt__get_current_slide', 'ppt__save_speaker_notes')

    def fallback_catalog(self, semantic_only):
        original={'messages':[{'role':'user','content':'这一页主要讲了什么？'}],
            'tools':[{'type':'function','function':{'name':name,
                'description':'Native presentation tool',
                'parameters':{'type':'object','properties':{},'required':[]}}}
                for name in self.NAMES]}
        result=json.loads(rewrite_chat_request(json.dumps(original).encode(),
            'test-model', self.NAMES, system_prompt_override='你是小K。',
            semantic_only=semantic_only))
        return original,result

    def test_semantic_chat_fallback_cannot_open_or_save_presentation(self):
        original,result=self.fallback_catalog(semantic_only=True)
        # Planning is performed against the original native catalog before this
        # fallback rewrite. Withhold side effects only from the delegated chat;
        # the read-only current-page tool remains available for explanations.
        self.assertEqual([tool['function']['name'] for tool in result['tools']],
                         ['ppt__get_current_slide'])
        self.assertEqual([tool['function']['name'] for tool in original['tools']],
                         list(self.NAMES))

    def test_regular_native_tool_backend_keeps_full_presentation_catalog(self):
        original,result=self.fallback_catalog(semantic_only=False)
        self.assertEqual(result['tools'],original['tools'])


if __name__=='__main__':unittest.main()
